#include <parquet/arrow/reader.h>
#include <fstream>
#include <iostream>
#include <sstream>

#include <gperftools/profiler.h>
#include <parquet/api/reader.h>
#include <thread>
#include "arrow/array.h"
#include "arrow/array/builder_binary.h"
#include "arrow/array/builder_primitive.h"
#include "arrow/buffer_builder.h"
#include "arrow/compute/api_aggregate.h"
#include "arrow/compute/kernels/util_internal.h"
#include "arrow/csv/api.h"
#include "arrow/dataset/api.h"
#include "arrow/dataset/discovery.h"
#include "arrow/dataset/file_base.h"
#include "arrow/filesystem/api.h"
#include "arrow/io/file.h"
#include "arrow/table.h"
#include "arrow/type_fwd.h"
#include "arrow/util/rle_encoding.h"
#include "json.hpp"
#include "parquet/arrow/writer.h"
#include "parquet/encoding.h"
#include "parquet/exception.h"
#include "parquet/file_reader.h"
#include "parquet/properties.h"
#include "parquet/statistics.h"
#include "stats.h"

using namespace arrow;
typedef std::chrono::duration<double> fsec;
typedef std::chrono::high_resolution_clock Time;
const int DATA_SIZE = 200 * 1000 * 1000;
const int BATCH_SIZE = DATA_SIZE;
uint32_t ROW_GROUP_SIZE = 10000000;  // Note: can be determined by input params
std::string source_file;
std::string second_file;
bool bitmap_filter = false;

std::string get_pq_name(parquet::Encoding::type encoding) {
  return "./encoding" + std::to_string(encoding) + "_rowgroup" +
         std::to_string(ROW_GROUP_SIZE) + "_datasize" + std::to_string(DATA_SIZE) +"_block"+ std::to_string(parquet::kForBlockSize)+
         source_file + ".parquet";
}
std::string get_pq_name_mulcol(parquet::Encoding::type encoding) {
  return "./encoding" + std::to_string(encoding) + "_rowgroup" +
         std::to_string(ROW_GROUP_SIZE) + "_datasize" + std::to_string(DATA_SIZE) +
         source_file+"_"+second_file + ".parquet";
}
arrow::Status encoder_decoder_test(parquet::Encoding::type encoding) {
  std::vector<int64_t> data;
  for (int64_t i = 0; i < DATA_SIZE; ++i) {
    data.push_back(i);
  }
  // ColumnDescriptor is only used for FIXED_LEN_BYTE_ARRAY
  auto encoder = parquet::MakeTypedEncoder<parquet::Int64Type>(
      encoding,
      /*use_dictionary=*/false, /*ColumnDescriptor* descr=*/nullptr);
  // seems that implement this func is enough for non-null case
  for (int32_t i = 0; i < DATA_SIZE / 1024; ++i) {
    encoder->Put(&data[i * 1024], 1024);
  }
  encoder->Put(&data[DATA_SIZE - DATA_SIZE % 1024], DATA_SIZE % 1024);
  auto buffer = encoder->FlushValues();
  std::vector<int64_t> decoded_data(DATA_SIZE);
  auto begin = stats::Time::now();
  auto decoder = parquet::MakeTypedDecoder<parquet::Int64Type>(
      encoding, /*ColumnDescriptor* descr=*/nullptr);
  decoder->SetData(DATA_SIZE, buffer->data(), static_cast<int>(buffer->size()));
  decoder->Decode(&decoded_data[0], DATA_SIZE);
  stats::cout_sec(begin, "decode ");
  for (size_t i = 0; i < DATA_SIZE; ++i) {
    if (data[i] != decoded_data[i]) {
      std::cout << data[i] << " " << decoded_data[i] << std::endl;
    }
  }
  return arrow::Status::OK();
}

arrow::Status full_scan_test(parquet::Encoding::type encoding) {
  std::vector<int32_t> a;
  for (int32_t i = 0; i < DATA_SIZE; ++i) {
    a.push_back(i);
  }
  std::vector<int32_t> b;
  for (int32_t i = DATA_SIZE; i > 0; --i) {
    b.push_back(i);
  }

  auto schema = arrow::schema({arrow::field("a", arrow::int32())});

  arrow::Int32Builder aBuilder;
  PARQUET_THROW_NOT_OK(aBuilder.AppendValues(a));

  // arrow::Int32Builder bBuilder;
  // PARQUET_THROW_NOT_OK(bBuilder.AppendValues(b));

  // arrow::BinaryBuilder cBuilder;
  // PARQUET_THROW_NOT_OK(cBuilder.AppendValues({"a", "b", "c", "d", "e", "f", "g",
  // "h"}));

  std::shared_ptr<arrow::Array> array_a;
  // std::shared_ptr<arrow::Array> array_a, array_b;
  ARROW_ASSIGN_OR_RAISE(array_a, aBuilder.Finish());
  // ARROW_ASSIGN_OR_RAISE(array_b, bBuilder.Finish());

  std::shared_ptr<arrow::Table> table = arrow::Table::Make(schema, {array_a});
  // std::shared_ptr<arrow::Table> table = arrow::Table::Make(schema, {array_a, array_b});
  uint32_t row_group_size = ROW_GROUP_SIZE;          // 64M / 10
  uint32_t dictionary_pages_size = 1 * 1024 * 1024;  // 64M * 0.03
  arrow::Compression::type codec = arrow::Compression::UNCOMPRESSED;
  std::shared_ptr<parquet::WriterProperties> properties =
      parquet::WriterProperties::Builder()
          .dictionary_pagesize_limit(dictionary_pages_size)
          ->compression(codec)
          ->disable_dictionary()
          ->encoding(encoding)
          ->version(parquet::ParquetVersion::PARQUET_2_LATEST)
          ->data_page_version(parquet::ParquetDataPageVersion::V2)
          ->build();

  std::shared_ptr<arrow::io::FileOutputStream> outfile;
  std::string parquet_name = "./encoding" + std::to_string(encoding) + "_rowgroup" +
                             std::to_string(row_group_size) + "_datasize" +
                             std::to_string(DATA_SIZE) + ".parquet";
  PARQUET_ASSIGN_OR_THROW(outfile, arrow::io::FileOutputStream::Open(parquet_name));

  PARQUET_THROW_NOT_OK(parquet::arrow::WriteTable(*table, arrow::default_memory_pool(),
                                                  outfile, row_group_size, properties));
  PARQUET_THROW_NOT_OK(outfile->Close());
  ARROW_ASSIGN_OR_RAISE(auto input, arrow::io::ReadableFile::Open(
                                        parquet_name, arrow::default_memory_pool()));

  // Instantiate TableReader from input stream and options
  std::unique_ptr<parquet::arrow::FileReader> pq_reader;

  // Read table from file
  auto reader_fut = parquet::ParquetFileReader::OpenAsync(input);
  ARROW_ASSIGN_OR_RAISE(std::unique_ptr<parquet::ParquetFileReader> pq_file_reader,
                        reader_fut.MoveResult());

  std::vector<int> columns;
  parquet::ScanFileContentsCheck(columns, BATCH_SIZE, pq_file_reader.get(), a, b);
  return arrow::Status::OK();
}

arrow::Status data_gen(parquet::Encoding::type encoding, std::vector<uint64_t>& a,
                       std::vector<uint64_t>& b) {
  std::string parquet_name = get_pq_name(encoding);

  auto schema = arrow::schema({arrow::field("a", arrow::uint64())});

  arrow::UInt64Builder aBuilder;
  PARQUET_THROW_NOT_OK(aBuilder.AppendValues(a));

  // arrow::UInt64Builder bBuilder;
  // PARQUET_THROW_NOT_OK(bBuilder.AppendValues(b));
  // arrow::Status data_gen(parquet::Encoding::type encoding, std::vector<uint32_t>& a,
  //                        std::vector<uint32_t>& b) {
  //   std::string parquet_name = get_pq_name(encoding);

  //   auto schema = arrow::schema(
  //       {arrow::field("a", arrow::uint32()), arrow::field("b", arrow::uint32())});

  //   arrow::UInt32Builder aBuilder;
  //   PARQUET_THROW_NOT_OK(aBuilder.AppendValues(a));

  //   arrow::UInt32Builder bBuilder;
  //   PARQUET_THROW_NOT_OK(bBuilder.AppendValues(b));

  std::shared_ptr<arrow::Array> array_a;
  ARROW_ASSIGN_OR_RAISE(array_a, aBuilder.Finish());
  // ARROW_ASSIGN_OR_RAISE(array_b, bBuilder.Finish());

  std::shared_ptr<arrow::Table> table = arrow::Table::Make(schema, {array_a});
  std::shared_ptr<arrow::io::FileOutputStream> outfile;
  PARQUET_ASSIGN_OR_THROW(outfile, arrow::io::FileOutputStream::Open(parquet_name));

  uint32_t row_group_size = ROW_GROUP_SIZE;          // 64M / 10
  uint32_t dictionary_pages_size = 1 * 1024 * 1024;  // 64M * 0.03
  arrow::Compression::type codec = arrow::Compression::UNCOMPRESSED;
  std::shared_ptr<parquet::WriterProperties> properties;
  auto builder = parquet::WriterProperties::Builder()
                     .dictionary_pagesize_limit(dictionary_pages_size)
                     ->compression(codec)
                     ->version(parquet::ParquetVersion::PARQUET_2_LATEST)
                     ->data_page_version(parquet::ParquetDataPageVersion::V2);
  if (encoding == parquet::Encoding::RLE_DICTIONARY) {
    properties = builder->enable_dictionary()
                     ->encoding(parquet::Encoding::PLAIN)
                     //  ->dictionary_pagesize_limit(512 * 1024 * 1024)
                     ->build();
  } else {
    properties = builder->disable_dictionary()->encoding(encoding)->build();
  }
  PARQUET_THROW_NOT_OK(parquet::arrow::WriteTable(*table, arrow::default_memory_pool(),
                                                  outfile, row_group_size, properties));
  PARQUET_THROW_NOT_OK(outfile->Close());
  return arrow::Status::OK();
}

arrow::Status async_read_file_w_zonemap_check(parquet::ParquetFileReader* reader,
                                              int64_t filter_val) {
  arrow::io::IOContext io_context = arrow::io::default_io_context();
  arrow::io::CacheOptions cache_options = arrow::io::CacheOptions::Defaults();
  for (int j = 0; j < reader->metadata()->num_columns(); j++) {
    for (int i = 0; i < reader->metadata()->num_row_groups(); i++) {
      auto group_reader = reader->RowGroup(i);
      auto column_chunk = group_reader->metadata()->ColumnChunk(0);
      int64_t min = 0;
      int64_t max = 0;
      std::shared_ptr<parquet::Statistics> stats = column_chunk->statistics();
      parquet::Int64Statistics* int_stats =
          dynamic_cast<parquet::Int64Statistics*>(stats.get());
      if (int_stats != nullptr) {
        min = int_stats->min();
        max = int_stats->max();
      }

      if (max > filter_val) {
        reader->PreBuffer({i}, {j}, io_context, cache_options);
      }
      // reader->PreBuffer({i}, {j}, io_context, cache_options);
      reader->WhenBuffered({i}, {j}).Wait();
    }
  }
  return arrow::Status::OK();
}

arrow::Status bitmap_scan(parquet::Encoding::type encoding,
                        std::vector<uint32_t>* bitpos = nullptr) {
  std::string parquet_name = "/root/arrow-private/cpp/out/build/leco-release/release/"+get_pq_name(encoding);
  std::cout<<parquet_name<<std::endl;
  std::vector<uint8_t> value_return(sizeof(uint64_t)*DATA_SIZE);
  auto begin = stats::Time::now();
  ARROW_ASSIGN_OR_RAISE(auto input, arrow::io::ReadableFile::Open(
                                        parquet_name, arrow::default_memory_pool()));

  // Instantiate TableReader from input stream and options
  std::unique_ptr<parquet::arrow::FileReader> pq_reader;

  // Read table from file
  auto reader_fut = parquet::ParquetFileReader::OpenAsync(input);
  ARROW_ASSIGN_OR_RAISE(std::unique_ptr<parquet::ParquetFileReader> pq_file_reader,
                        reader_fut.MoveResult());
  double computetime = 0;

  std::vector<int> columns = {0,1};
  if (bitpos == nullptr) {
    parquet::ScanFileContents(columns, BATCH_SIZE, pq_file_reader.get());
  } else {
    // if (encoding == parquet::Encoding::RLE_DICTIONARY
    //     // ||encoding == parquet::Encoding::PLAIN
    // ) {
    //   parquet::ScanFileContentsBitposDict(columns, BATCH_SIZE, pq_file_reader.get(),
    //                                       *bitpos);
    // } else {
    parquet::ScanFileContentsBitpos(columns, BATCH_SIZE, pq_file_reader.get(), *bitpos, value_return.data(), &computetime);
    // }
  }
  stats::cout_sec(begin, "pure scan " + std::to_string(encoding));
  return arrow::Status::OK();
}

arrow::Status filter_scan(parquet::Encoding::type encoding, int64_t filter_val,
                          std::vector<uint32_t>& bitpos, bool async_read, bool is_gt = true, int64_t filter2=0,int64_t base_val=0) {
  // filter by > filter_val and return the position of matched rows
  if(!is_gt){
    assert(filter2>filter_val);
  }
  std::string parquet_name = "/mnt/"+get_pq_name(encoding);
  std::vector<uint8_t> value_return(sizeof(uint64_t)*DATA_SIZE);
  if(bitmap_filter){
    parquet_name =  "/mnt/"+get_pq_name_mulcol(encoding);
  }
  // std::cout<<"parquet_name: "<<parquet_name<<std::endl;
  ARROW_ASSIGN_OR_RAISE(auto input, arrow::io::ReadableFile::Open(
                                        parquet_name, arrow::default_memory_pool()));

  // Instantiate TableReader from input stream and options
  std::unique_ptr<parquet::arrow::FileReader> pq_reader;

  // Read table from file
  auto reader_fut = parquet::ParquetFileReader::OpenAsync(input);
  ARROW_ASSIGN_OR_RAISE(std::unique_ptr<parquet::ParquetFileReader> pq_file_reader,
                        reader_fut.MoveResult());
  std::thread th1;
  if (async_read) {
    th1 = std::thread(async_read_file_w_zonemap_check, pq_file_reader.get(), filter_val);
    th1.join();
  }
  // system("cat /proc/$PPID/io");
  int64_t number_remains = 0;
  double compute_time = 0;
  int N = bitpos.size();
  auto begin = stats::Time::now();
  std::vector<int> columns = {0};
  // if (encoding == parquet::Encoding::RLE_DICTIONARY
  //       ||encoding == parquet::Encoding::PLAIN
  //   ) {
  //     parquet::FilterScanFileContentsDict(columns, BATCH_SIZE, pq_file_reader.get(), filter_val,
  //                                 bitpos, is_gt, &number_remains, filter2, &compute_time, base_val, encoding);
  //   }
  // else{
    parquet::FilterScanFileContents(columns, BATCH_SIZE, pq_file_reader.get(), filter_val,
                                  bitpos, is_gt, &number_remains, filter2, &compute_time, base_val, encoding);
  // }
  // system("cat /proc/$PPID/io");
  // stats::cout_sec(begin, "filter scan " + std::to_string(encoding));
  std::cout<<number_remains<<std::endl;
  if(bitmap_filter){
    // std::cout<<"compute time: "<<compute_time<<std::endl;
      bitpos.resize(number_remains);
      std::vector<int> bit_columns = {1};
      // begin = stats::Time::now();
  //       if (encoding == parquet::Encoding::RLE_DICTIONARY
  //       ||encoding == parquet::Encoding::PLAIN
  //   ) {
  //     parquet::ScanFileContentsBitposDict(bit_columns, BATCH_SIZE, pq_file_reader.get(), bitpos, value_return.data(), &compute_time);
  //   }
  // else{
      parquet::ScanFileContentsBitpos(bit_columns, BATCH_SIZE, pq_file_reader.get(), bitpos, value_return.data(), &compute_time);
  // }
  }
  // stats::cout_sec(begin, "filter scan " + std::to_string(encoding));
  double total_time = ((fsec)(Time::now() - begin)).count();
  std::cout<< encoding<<" "<<async_read<<" "<<(double)number_remains/(double)N<<" "<<total_time<<" "<< total_time-compute_time<<std::endl;

  return arrow::Status::OK();
}

// arrow::Status get_src_file(std::vector<uint32_t>& data, std::string& src_file) {
arrow::Status get_src_file(std::vector<uint64_t>& data, std::string& src_file) {
  std::ifstream srcFile("/root/arrow-private/cpp/Learn-to-Compress/data/" + src_file,
                        std::ios::in);
  if (!srcFile) {
    return arrow::Status::UnknownError("error opening source file.");
  }
  while (1) {
    uint64_t next;
    srcFile >> next;
    if (srcFile.eof()) {
      break;
    }
    data.push_back(next);
  }
  srcFile.close();
  return arrow::Status::OK();
}

template <typename T>
static std::vector<T> load_data_binary(const std::string& filename, bool print = true) {
  std::vector<T> data;

  std::ifstream in(filename, std::ios::binary);
  if (!in.is_open()) {
    std::cerr << "unable to open " << filename << std::endl;
    exit(EXIT_FAILURE);
  }
  // Read size.
  uint64_t size;
  in.read(reinterpret_cast<char*>(&size), sizeof(uint64_t));
  data.resize(size);
  // Read values.
  in.read(reinterpret_cast<char*>(data.data()), size * sizeof(T));
  in.close();

  return data;
}

// Example Usage: /root/arrow-private/cpp/out/build/leco-release/release/for FOR 1
// normal_200M_uint32.txt bitmap_random_0.01_200000000.txt 0
arrow::Status RunMain(int argc, char** argv) {
  std::string encoding = argv[1];
  source_file = std::string(argv[2]);
  bool gen_data_flag = std::stoi(argv[3]);
  bool use_async_io = std::stoi(argv[4]);
  int64_t filter_val = 0;
  int64_t filter2 = INT64_MAX;
  int64_t base_val = 0;
  bool open_range = true;
  if (argc > 5) {
    ROW_GROUP_SIZE = std::stoi(argv[5]);
  }
  if (argc > 6) {
    parquet::kForBlockSize = std::stoi(argv[6]);
  }
  if (argc > 7) {
    filter_val = std::atoll(argv[7]);
  }
  if (argc > 8) {
    open_range = false;
    filter2 = std::atoll(argv[8]);
  }
  if(argc>9){
    base_val = std::atoll(argv[9]);
  }
  if (argc > 10) {
    second_file = std::string(argv[10]);
    bitmap_filter = true;
  }
  // std::vector<uint32_t> bitpos_vec;
  std::vector<uint32_t> vec_ptr(DATA_SIZE);

  // ARROW_RETURN_NOT_OK(encoder_decoder_test(parquet::Encoding::PLAIN));
  // ARROW_RETURN_NOT_OK(encoder_decoder_test(parquet::Encoding::FOR));
  // ARROW_RETURN_NOT_OK(encoder_decoder_test(parquet::Encoding::LECO));
  // std::cout << "finish encoding test" << std::endl;
  // ARROW_RETURN_NOT_OK(full_scan_test(parquet::Encoding::PLAIN));
  // ARROW_RETURN_NOT_OK(full_scan_test(parquet::Encoding::FOR));
  // ARROW_RETURN_NOT_OK(full_scan_test(parquet::Encoding::LECO));

  if (gen_data_flag) {
    // begin src file in
    std::vector<uint64_t> data;
    // std::vector<uint32_t> data;
    if (source_file == "fb_200M_uint64"|| source_file == "wiki_200M_uint64") {
      auto data_64 = load_data_binary<uint64_t>(
          "/root/arrow-private/cpp/Learn-to-Compress/data/" + source_file);
      for (auto& d : data_64) {
        data.emplace_back(d);
      }
    } else {
      PARQUET_THROW_NOT_OK(get_src_file(data, source_file));
    }

    if (encoding == "PLAIN") {
      ARROW_RETURN_NOT_OK(data_gen(parquet::Encoding::PLAIN, data, data));
    } else if (encoding == "FOR") {
      ARROW_RETURN_NOT_OK(data_gen(parquet::Encoding::FOR, data, data));
    } else if (encoding == "LECO") {
      ARROW_RETURN_NOT_OK(data_gen(parquet::Encoding::LECO, data, data));
    } else if (encoding == "DELTA") {
      ARROW_RETURN_NOT_OK(data_gen(parquet::Encoding::DELTA, data, data));
    } else if (encoding == "DICT") {
      ARROW_RETURN_NOT_OK(data_gen(parquet::Encoding::RLE_DICTIONARY, data, data));
    } else {
      std::cout << "wrong encoding" << std::endl;
    }
  } else {
    if (encoding == "PLAIN") {
      ARROW_RETURN_NOT_OK(filter_scan(parquet::Encoding::PLAIN, filter_val, vec_ptr, use_async_io, open_range, filter2, base_val));
    } else if (encoding == "FOR") {
      ARROW_RETURN_NOT_OK(filter_scan(parquet::Encoding::FOR, filter_val, vec_ptr, use_async_io, open_range, filter2, base_val));
    } else if (encoding == "LECO") {
      ARROW_RETURN_NOT_OK(filter_scan(parquet::Encoding::LECO, filter_val, vec_ptr, use_async_io, open_range, filter2, base_val));
    } else if (encoding == "DELTA") {
      ARROW_RETURN_NOT_OK(filter_scan(parquet::Encoding::DELTA, filter_val, vec_ptr, use_async_io, open_range, filter2, base_val));
    } else if (encoding == "DICT") {
      ARROW_RETURN_NOT_OK(
          filter_scan(parquet::Encoding::RLE_DICTIONARY, filter_val, vec_ptr, use_async_io, open_range, filter2, base_val));
    } else {
      std::cout << "wrong encoding" << std::endl;
    }
  }
  // system("cat /proc/$PPID/io");
  return arrow::Status::OK();
}

int main(int argc, char** argv) {
  arrow::Status status = RunMain(argc, argv);
  if (!status.ok()) {
    std::cerr << status << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}