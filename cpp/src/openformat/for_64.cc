#include <parquet/arrow/reader.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include<algorithm>

#include <gperftools/profiler.h>
#include <parquet/api/reader.h>
#include "arrow/array.h"
#include "arrow/array/data.h"
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
#include "arrow/compute/api_vector.h"
#include "arrow/testing/random.h"


#include "json.hpp"
#include "parquet/arrow/writer.h"
#include "parquet/encoding.h"
#include "parquet/exception.h"
#include "parquet/file_reader.h"
#include "parquet/column_reader.h"
#include "parquet/properties.h"
#include "stats.h"
#include <random>
#include <algorithm>
using namespace arrow;
using arrow::ArraySpan;

const int DATA_SIZE = 200000000;
const int BATCH_SIZE = DATA_SIZE;
uint32_t ROW_GROUP_SIZE = 10000000;  // Note: can be determined by input params
std::string source_file;
std::string second_file;
typedef std::chrono::duration<double> fsec;
typedef std::chrono::high_resolution_clock Time;

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

arrow::Status data_gen_multicol(parquet::Encoding::type encoding, std::vector<uint64_t>& a,
                       std::vector<uint64_t>& b) {
                        
  std::string parquet_name = "/mnt/"+get_pq_name_mulcol(encoding);

  auto schema = arrow::schema({arrow::field("a", arrow::uint64()), arrow::field("b", arrow::uint64())});
  // auto schema = arrow::schema({arrow::field("a", arrow::uint64())});

  arrow::UInt64Builder aBuilder;
  PARQUET_THROW_NOT_OK(aBuilder.AppendValues(a));

  arrow::UInt64Builder bBuilder;
  PARQUET_THROW_NOT_OK(bBuilder.AppendValues(b));

  std::shared_ptr<arrow::Array> array_a;
  ARROW_ASSIGN_OR_RAISE(array_a, aBuilder.Finish());
  std::shared_ptr<arrow::Array> array_b;
  ARROW_ASSIGN_OR_RAISE(array_b, bBuilder.Finish());

  std::shared_ptr<arrow::Table> table = arrow::Table::Make(schema, {array_a, array_b});
  // std::shared_ptr<arrow::Table> table = arrow::Table::Make(schema, {array_a});
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

arrow::Status data_gen(parquet::Encoding::type encoding, std::vector<uint64_t>& a,
                       std::vector<uint64_t>& b) {
  if(b.size()>0){
    return data_gen_multicol(encoding, a, b);
  }
  std::string parquet_name = "/mnt/"+get_pq_name(encoding);

  // auto schema = arrow::schema({arrow::field("a", arrow::uint64()), arrow::field("b", arrow::uint64())});
  auto schema = arrow::schema({arrow::field("a", arrow::uint64())});

  arrow::UInt64Builder aBuilder;
  PARQUET_THROW_NOT_OK(aBuilder.AppendValues(a));

  // arrow::UInt64Builder bBuilder;
  // PARQUET_THROW_NOT_OK(bBuilder.AppendValues(b));

  std::shared_ptr<arrow::Array> array_a;
  ARROW_ASSIGN_OR_RAISE(array_a, aBuilder.Finish());
  // std::shared_ptr<arrow::Array> array_b;
  // ARROW_ASSIGN_OR_RAISE(array_b, bBuilder.Finish());

  // std::shared_ptr<arrow::Table> table = arrow::Table::Make(schema, {array_a, array_b});
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

int random_int(int m)
{
    return rand() % m;
}
arrow::Status pure_scan(parquet::Encoding::type encoding,
                        std::vector<uint32_t>* bitpos = nullptr, bool pushdown = false) {
  auto ctx = arrow::compute::default_exec_context();
  std::string parquet_name = "/mnt/"+get_pq_name(encoding);
  // std::cout<<parquet_name<<std::endl;
  std::vector<uint8_t> value_return(sizeof(uint64_t)*DATA_SIZE);
  std::vector<std::shared_ptr<ArraySpan>> total_array;
  std::shared_ptr<ArraySpan> whole_array;
  std::vector<std::shared_ptr<parquet::ColumnReader>> col_readers;
  int row_group_num = DATA_SIZE/ROW_GROUP_SIZE;
  total_array.reserve(row_group_num);
  col_readers.reserve(row_group_num);

  std::vector<std::vector<int>> pos;
  std::vector<int> pos_total;
  pos.reserve(row_group_num);
  if(bitpos){
    for(auto item: *bitpos){
      pos[int(item/ROW_GROUP_SIZE)].push_back(item%ROW_GROUP_SIZE);
      pos_total.push_back(item);
    }
  }

  std::random_device rd;  // Create a random seed
  std::mt19937 gen(rd()); // Mersenne Twister pseudo-random generator
  std::shuffle(pos_total.begin(), pos_total.end(), gen);
  
  

  std::vector<std::shared_ptr<Array>> indices;
  for(int i = 0; i< row_group_num; i++){
    std::shared_ptr<Buffer> array_tmp_compress = std::make_shared<Buffer>(reinterpret_cast<uint8_t*>(pos[i].data()),pos[i].size()*sizeof(int));
    std::vector<std::shared_ptr<Buffer>> buffers_compress = {nullptr, array_tmp_compress};
    std::shared_ptr<ArrayData> indicedata_compress;

    indicedata_compress = std::make_shared<ArrayData>(::arrow::int32(), pos[i].size(),
                                                  std::move(buffers_compress), /*null_count=*/0, 0, CODEC::PLAIN);
    ArraySpan indice_com(*indicedata_compress);
    indices.push_back(std::move(indice_com.ToArray()));
  }


  std::shared_ptr<Buffer> array_tmp_compress = std::make_shared<Buffer>(reinterpret_cast<uint8_t*>(pos_total.data()),pos_total.size()*sizeof(int));
  std::vector<std::shared_ptr<Buffer>> buffers_compress = {nullptr, array_tmp_compress};
  std::shared_ptr<ArrayData> indicedata_compress;
  indicedata_compress = std::make_shared<ArrayData>(::arrow::int32(), pos_total.size(),
                                                std::move(buffers_compress), /*null_count=*/0, 0, CODEC::PLAIN);
  ArraySpan indice_com(*indicedata_compress);
  std::shared_ptr<Array> indices_total = indice_com.ToArray();


  auto begin = stats::Time::now();
  ARROW_ASSIGN_OR_RAISE(auto input, arrow::io::ReadableFile::Open(
                                        parquet_name, arrow::default_memory_pool()));

  // Instantiate TableReader from input stream and options
  std::unique_ptr<parquet::arrow::FileReader> pq_reader;

  // Read table from file
  auto reader_fut = parquet::ParquetFileReader::OpenAsync(input);
  ARROW_ASSIGN_OR_RAISE(std::unique_ptr<parquet::ParquetFileReader> pq_file_reader,
                        reader_fut.MoveResult());
  double compute_time = 0;
  std::vector<int> columns = {0};
  if (!pushdown) {
    // if (encoding == parquet::Encoding::PLAIN){
    //   parquet::ScanFileContents(columns, BATCH_SIZE, pq_file_reader.get(),  value_return.data());
    // }
    //  parquet::ScanFileContentsWholeArrow(columns, BATCH_SIZE, pq_file_reader.get(), value_return.data(), whole_array, &col_readers);
     parquet::ScanFileContentsArrow(columns, BATCH_SIZE, pq_file_reader.get(), value_return.data(), &total_array, &col_readers, &compute_time);
  } else {
    // if (encoding == parquet::Encoding::RLE_DICTIONARY
    //     ||encoding == parquet::Encoding::PLAIN
    // ) {
    //   parquet::ScanFileContentsBitposDict(columns, BATCH_SIZE, pq_file_reader.get(),
    //                                       *bitpos);
    // } else {
    parquet::ScanFileContentsBitpos(columns, BATCH_SIZE, pq_file_reader.get(), *bitpos, value_return.data(),&compute_time);
    // }
  }
  // std::cout<<compute_time<<std::endl;
  // uint64_t tmpsum = 0;
  // if(bitpos==nullptr){
  // int64_t* value_tmp = reinterpret_cast<int64_t*>(value_return.data());
  // if (encoding == parquet::Encoding::PLAIN){
  //   for(auto idx: pos){
  //     tmpsum+=value_tmp[idx];
  //   }
  // }
  // else{
  // for(auto idx: pos){
  //   tmpsum+=total_array[(int)idx/ROW_GROUP_SIZE]->GetSingleValue<int64_t>(1,idx%ROW_GROUP_SIZE);
  // }
  // }
  // }
  // if(encoding == parquet::Encoding::PLAIN){
  //   std::shared_ptr<Buffer> array_result= std::make_shared<Buffer>(value_return.data(),value_return.size());
  //   std::vector<std::shared_ptr<Buffer>> buffers = {nullptr, array_result};
  //   std::shared_ptr<ArrayData> arrays;
  //   arrays = std::make_shared<ArrayData>(::arrow::int64(), DATA_SIZE,
  //                                               std::move(buffers), /*null_count=*/0, 0, CODEC::PLAIN);
  //   ArraySpan array_output(*arrays);                                        
  //   std::shared_ptr<Array> tmparray = array_output.ToArray();
  //   std::shared_ptr<Array> indicearray = indice.ToArray();
  //   auto result = arrow::compute::Take(tmparray, indicearray, arrow::compute::TakeOptions::Defaults(), ctx);
  //   const int64_t* result_sq = result->array()->GetValues<int64_t>(1);
  //   std::cout<<result_sq[0]<<std::endl;
  // }
  // else{
  
  // ************ split AA ****************
  //   for(int i =0;i<row_group_num;i++){
  //       std::shared_ptr<Array> tmparray = total_array[i]->ToArray();
  //       auto result = arrow::compute::Take(tmparray, indices[i], arrow::compute::TakeOptions::Defaults(), ctx);
  //       const int64_t* result_sq = result->array()->GetValues<int64_t>(1);
  //       // std::cout<<result_sq[0]<<std::endl;
  //   // }
  // }
  // ************ Chunked CA ****************
  std::vector<std::shared_ptr<Array>> total_array_vector;
  for(auto item: total_array){
    total_array_vector.push_back(std::move(item->ToArray()));
  }
  ChunkedArray chunkedarray(total_array_vector);
  auto result = arrow::compute::Take(chunkedarray.Slice(0), indices_total, arrow::compute::TakeOptions::Defaults(), ctx);
  const int64_t* result_sq = result->chunked_array()->chunks()[0]->data()->GetValues<int64_t>(1);
  std::cout<<result_sq[0]<<std::endl;

  

  double total_time = ((fsec)(Time::now() - begin)).count();
  std::cout<< encoding<<" "<<total_time<<" "<< total_time-compute_time<<std::endl;
  // std::cout<<tmpsum<<std::endl;
  
  

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

arrow::Status get_src_file_32(std::vector<uint32_t>& data, std::string& src_file) {
  std::ifstream srcFile("/root/arrow-private/cpp/Learn-to-Compress/data/" + src_file,
                        std::ios::in);
  if (!srcFile) {
    return arrow::Status::UnknownError("error opening source file.");
  }
  while (srcFile.good()) {
    uint32_t next;
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
static std::vector<T> load_data_binary(const std::string &filename,
                                       bool print = true)
{
    std::vector<T> data;

    std::ifstream in(filename, std::ios::binary);
    if (!in.is_open())
    {
        std::cerr << "unable to open " << filename << std::endl;
        exit(EXIT_FAILURE);
    }
    // Read size.
    uint64_t size;
    in.read(reinterpret_cast<char *>(&size), sizeof(uint64_t));
    data.resize(size);
    // Read values.
    in.read(reinterpret_cast<char *>(data.data()), size * sizeof(T));
    in.close();

    return data;
}

// Example Usage: /root/arrow-private/cpp/out/build/leco-release/release/for FOR 1
// normal_200M_uint32.txt bitmap_random_0.01_200000000.txt 0
arrow::Status RunMain(int argc, char** argv) {
  std::string encoding = argv[1];
  bool do_bitpos_flag = std::stoi(argv[2]);
  source_file = std::string(argv[3]);
  std::string bitmap_name = std::string(argv[4]);
  bool gen_data_flag = std::stoi(argv[5]);
  if (argc > 6) {
    ROW_GROUP_SIZE = std::stoi(argv[6]) ;
  }
  if (argc > 7) {
    parquet::kForBlockSize = std::stoi(argv[7]);
  }
  if (argc > 8) {
    second_file = std::string(argv[8]);
  }
  std::vector<uint32_t> bit_pos;
  // std::vector<uint32_t> bitpos_vec;
  std::vector<uint32_t>* vec_ptr;
  if (do_bitpos_flag) {
    vec_ptr = &bit_pos;
  } else {
    vec_ptr = nullptr;
  }
  // begin bitmap file in
  std::ifstream bitFile(
      "/root/arrow-private/cpp/Learn-to-Compress/data/bitmap_random_cluster/" +
          bitmap_name,
      std::ios::in);
  std::cout << "../data/bitmap_random/" + bitmap_name << std::endl;
  for (int i = 0;; i++) {
    uint32_t next;
    bitFile >> next;
    if (bitFile.eof()) {
      break;
    }
    if (next) {
      bit_pos.emplace_back(i);
    }
  }
  bitFile.close();
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
    std::vector<uint64_t> data_2;
    // std::vector<uint64_t> tmp_data2;
    PARQUET_THROW_NOT_OK(get_src_file(data, source_file));

    if (second_file == "fb_200M_uint64"||second_file == "wiki_200M_uint64") {
      data_2 = load_data_binary<uint64_t>(
          "/root/arrow-private/cpp/Learn-to-Compress/data/" + second_file);
      // for (auto d : data_64) {
      //   data_2.emplace_back(d);
      // }
        std::random_device rd;
        std::mt19937 rng(rd());
        std::shuffle(data_2.begin(), data_2.end(), rng);
    } else {
      if(argc>8){
        PARQUET_THROW_NOT_OK(get_src_file(data_2, second_file));
        std::random_device rd;
        std::mt19937 rng(rd());
        std::shuffle(data_2.begin(), data_2.end(), rng);
      }
      
    }
    // for(auto item: tmp_data2){
    //   data_2.push_back(static_cast<uint32_t>(item));
    // }
    

    

    if (encoding == "PLAIN") {
      ARROW_RETURN_NOT_OK(data_gen(parquet::Encoding::PLAIN, data, data_2));
    } else if (encoding == "FOR") {
      ARROW_RETURN_NOT_OK(data_gen(parquet::Encoding::FOR, data, data_2));
    } else if (encoding == "LECO") {
      ARROW_RETURN_NOT_OK(data_gen(parquet::Encoding::LECO, data, data_2));
    } else if (encoding == "DELTA") {
      ARROW_RETURN_NOT_OK(data_gen(parquet::Encoding::DELTA, data, data_2));
    } else if (encoding == "DICT") {
      ARROW_RETURN_NOT_OK(data_gen(parquet::Encoding::RLE_DICTIONARY, data, data_2));
    } else {
      std::cout << "wrong encoding" << std::endl;
    }
  } else {
    if (encoding == "PLAIN") {
      ARROW_RETURN_NOT_OK(pure_scan(parquet::Encoding::PLAIN, vec_ptr));
    } else if (encoding == "FOR") {
      ARROW_RETURN_NOT_OK(pure_scan(parquet::Encoding::FOR, vec_ptr));
    } else if (encoding == "LECO") {
      ARROW_RETURN_NOT_OK(pure_scan(parquet::Encoding::LECO, vec_ptr));
    } else if (encoding == "DELTA") {
      ARROW_RETURN_NOT_OK(pure_scan(parquet::Encoding::DELTA, vec_ptr));
    } else if (encoding == "DICT") {
      ARROW_RETURN_NOT_OK(pure_scan(parquet::Encoding::RLE_DICTIONARY, vec_ptr));
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