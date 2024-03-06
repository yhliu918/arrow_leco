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
#include "arrow/compute/api.h"
#include "arrow/compute/exec/aggregate.h"
#include "stats.h"

using namespace arrow;
typedef std::chrono::duration<double> fsec;
typedef std::chrono::high_resolution_clock Time;
using arrow::compute::internal::GroupBy;
using arrow::compute::Aggregate;
const int DATA_SIZE = 200 * 1000 * 1000;
const int BATCH_SIZE = DATA_SIZE;
uint32_t ROW_GROUP_SIZE = 10000000;  // Note: can be determined by input params
std::string source_file;
std::string groupby_key_file;
std::string sum_file;
bool bitmap_groupby = false;

std::string get_pq_name(parquet::Encoding::type encoding) {
  return "./encoding" + std::to_string(encoding) + "_rowgroup" +
         std::to_string(ROW_GROUP_SIZE) + "_datasize" + std::to_string(DATA_SIZE) +"_block"+ std::to_string(parquet::kForBlockSize)+
         source_file + ".parquet";
}
std::string get_pq_name_mulcol(parquet::Encoding::type encoding) {
  return "./encoding" + std::to_string(encoding) + "_rowgroup" +
         std::to_string(ROW_GROUP_SIZE) + "_datasize" + std::to_string(DATA_SIZE) +
         source_file+"_"+groupby_key_file+'_'+sum_file + ".parquet";
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

arrow::Status data_gen_multicol(parquet::Encoding::type encoding, std::vector<uint64_t>& a,
                       std::vector<uint64_t>& b, std::vector<uint64_t>& c) {
                        
  std::string parquet_name = "/mnt/"+get_pq_name_mulcol(encoding);

  auto schema = arrow::schema({arrow::field("a", arrow::uint64()), arrow::field("b", arrow::uint64()), arrow::field("c", arrow::uint64())});
  // auto schema = arrow::schema({arrow::field("a", arrow::uint64())});

  arrow::UInt64Builder aBuilder;
  PARQUET_THROW_NOT_OK(aBuilder.AppendValues(a));

  arrow::UInt64Builder bBuilder;
  PARQUET_THROW_NOT_OK(bBuilder.AppendValues(b));

  arrow::UInt64Builder cBuilder;
  PARQUET_THROW_NOT_OK(cBuilder.AppendValues(c));

  std::shared_ptr<arrow::Array> array_a;
  ARROW_ASSIGN_OR_RAISE(array_a, aBuilder.Finish());
  std::shared_ptr<arrow::Array> array_b;
  ARROW_ASSIGN_OR_RAISE(array_b, bBuilder.Finish());
  std::shared_ptr<arrow::Array> array_c;
  ARROW_ASSIGN_OR_RAISE(array_c, cBuilder.Finish());

  std::shared_ptr<arrow::Table> table = arrow::Table::Make(schema, {array_a, array_b, array_c});
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

void transform_bitmap(std::vector<uint32_t> bitpos, std::vector<uint8_t>* bitmap){
  for(int i =0; i< bitpos.size();i++){
    int pos = bitpos[i];
    (*bitmap)[pos/8] += (1<<(7-pos%8));
  }
}

std::shared_ptr<Array> ConcatArrays(std::vector<std::shared_ptr<ArraySpan>> array_seq, arrow::compute::ExecContext* ctx, std::shared_ptr<Buffer>& bitmap, int remain_num){
  std::vector<std::shared_ptr<Buffer>> buffers;
  for(auto item: array_seq){
    buffers.push_back(std::move(item->GetBuffer(1)));
  }
  std::shared_ptr<Buffer> array_buffer;
  ConcatenateBuffers(buffers, ctx->memory_pool(), array_seq[0]->compression_type, DATA_SIZE).Value(&array_buffer);
  std::vector<std::shared_ptr<Buffer>> buffers_total = {bitmap, array_buffer};
  std::shared_ptr<ArrayData> data = ArrayData::Make(::arrow::int64(), DATA_SIZE,
                                                std::move(buffers_total), remain_num, 0,array_seq[0]->compression_type);
  return MakeArray(data);
}

arrow::Status filter_groupby(parquet::Encoding::type encoding, int64_t filter_val,
                          std::vector<uint32_t>& bitpos, bool async_read, bool is_gt = true, int64_t filter2=0,int64_t base_val=0) {
  // filter by > filter_val and return the position of matched rows
  if(!is_gt){
    assert(filter2>filter_val);
  }
  auto ctx = arrow::compute::default_exec_context();
  std::string parquet_name = "/mnt/"+get_pq_name(encoding);
  std::vector<uint8_t> value_return(sizeof(uint64_t)*DATA_SIZE);
  if(bitmap_groupby){
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
  double filter_compute_time = 0;
  double groupagg_compute_time = 0;
  int N = bitpos.size();
  std::vector<uint8_t> bitmap;
  bitmap.reserve(DATA_SIZE/8);
  for(int i = 0; i< DATA_SIZE/8; i++){
    bitmap.push_back(0);
  }

  auto begin = stats::Time::now();
  std::vector<int> columns = {0};
    parquet::FilterScanFileContents(columns, BATCH_SIZE, pq_file_reader.get(), filter_val,
                                  bitpos, is_gt, &number_remains, filter2, &filter_compute_time, base_val, encoding);
  double total_time = ((fsec)(Time::now() - begin)).count();

  bitpos.resize(number_remains);
  transform_bitmap(bitpos, &bitmap);
  std::cout<<number_remains<<std::endl;

  begin = stats::Time::now();
  if(bitmap_groupby){
      std::vector<int> bit_columns = {1,2};
      std::vector<std::shared_ptr<ArraySpan>> total_array;
      std::vector<std::shared_ptr<parquet::ColumnReader>> col_readers;
      int row_group_num = DATA_SIZE/ROW_GROUP_SIZE;
      total_array.reserve(row_group_num*bit_columns.size());
      col_readers.reserve(row_group_num*bit_columns.size());
      parquet::ScanFileContentsArrow(bit_columns, BATCH_SIZE, pq_file_reader.get(), value_return.data(), &total_array, &col_readers, &groupagg_compute_time);
      total_time += ((fsec)(Time::now() - begin)).count();


      std::vector<std::shared_ptr<ArraySpan>> groupbykey;
      std::vector<std::shared_ptr<ArraySpan>> sumkey;
      for(int i = 0; i< total_array.size(); i++){
        if(i%2==0){
          groupbykey.push_back(std::move(total_array[i]));
        }
        else{
          sumkey.push_back(std::move(total_array[i]));
        }
      }
      std::shared_ptr<Buffer> array_tmp = std::make_shared<Buffer>(reinterpret_cast<uint8_t*>(bitmap.data()),DATA_SIZE/8);
      std::shared_ptr<Array> groupby_col = ConcatArrays(groupbykey, ctx, array_tmp, DATA_SIZE-number_remains);
      std::shared_ptr<Array> sum_col = ConcatArrays(sumkey, ctx, array_tmp, DATA_SIZE-number_remains);
      
      begin = stats::Time::now();
      auto groupby_begin = stats::Time::now();
      std::vector<Aggregate> aggregates;
      aggregates.push_back({"hash_sum", std::move(nullptr),
                          "agg_" + std::to_string(0),"hash_sum"});

      auto result = GroupBy({sum_col}, {groupby_col}, aggregates);
      groupagg_compute_time += ((fsec)(Time::now() - groupby_begin)).count();
  }
  // stats::cout_sec(begin, "filter scan " + std::to_string(encoding));
  total_time += ((fsec)(Time::now() - begin)).count();
  std::cout<< encoding<<" "<<async_read<<" "<<(double)number_remains/(double)N<<" "<<total_time<<" "<< total_time-groupagg_compute_time - filter_compute_time<<" "<<filter_compute_time<<" "<< groupagg_compute_time<<std::endl;

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

arrow::Status get_src_file_16(std::vector<uint16_t>& data, std::string& src_file) {
  std::ifstream srcFile("/root/arrow-private/cpp/Learn-to-Compress/data/" + src_file,
                        std::ios::in);
  if (!srcFile) {
    return arrow::Status::UnknownError("error opening source file.");
  }
  while (1) {
    uint16_t next;
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
    groupby_key_file = std::string(argv[10]);
    sum_file = std::string(argv[11]);
    bitmap_groupby = true;
  }
  // std::vector<uint32_t> bitpos_vec;
  std::vector<uint32_t> vec_ptr(DATA_SIZE);

  if (gen_data_flag) {
    // begin src file in
    std::vector<uint64_t> data;
    get_src_file(data, source_file);

    std::vector<uint64_t> datab;
    get_src_file(datab, groupby_key_file);

    std::vector<uint64_t> datac;
    get_src_file(datac, sum_file);
    

    if (encoding == "PLAIN") {
      ARROW_RETURN_NOT_OK(data_gen_multicol(parquet::Encoding::PLAIN, data, datab, datac));
    } else if (encoding == "FOR") {
      ARROW_RETURN_NOT_OK(data_gen_multicol(parquet::Encoding::FOR, data, datab, datac));
    } else if (encoding == "LECO") {
      ARROW_RETURN_NOT_OK(data_gen_multicol(parquet::Encoding::LECO, data, datab, datac));
    } else if (encoding == "DELTA") {
      ARROW_RETURN_NOT_OK(data_gen_multicol(parquet::Encoding::DELTA, data, datab, datac));
    } else if (encoding == "DICT") {
      ARROW_RETURN_NOT_OK(data_gen_multicol(parquet::Encoding::RLE_DICTIONARY, data, datab, datac));
    } else {
      std::cout << "wrong encoding" << std::endl;
    }
  } else {
    if (encoding == "PLAIN") {
      ARROW_RETURN_NOT_OK(filter_groupby(parquet::Encoding::PLAIN, filter_val, vec_ptr, use_async_io, open_range, filter2, base_val));
    } else if (encoding == "FOR") {
      ARROW_RETURN_NOT_OK(filter_groupby(parquet::Encoding::FOR, filter_val, vec_ptr, use_async_io, open_range, filter2, base_val));
    } else if (encoding == "LECO") {
      ARROW_RETURN_NOT_OK(filter_groupby(parquet::Encoding::LECO, filter_val, vec_ptr, use_async_io, open_range, filter2, base_val));
    } else if (encoding == "DELTA") {
      ARROW_RETURN_NOT_OK(filter_groupby(parquet::Encoding::DELTA, filter_val, vec_ptr, use_async_io, open_range, filter2, base_val));
    } else if (encoding == "DICT") {
      ARROW_RETURN_NOT_OK(
          filter_groupby(parquet::Encoding::RLE_DICTIONARY, filter_val, vec_ptr, use_async_io, open_range, filter2, base_val));
    } else {
      std::cout << "wrong encoding" << std::endl;
    }
  }
  system("cat /proc/$PPID/status |grep VmHWM");
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