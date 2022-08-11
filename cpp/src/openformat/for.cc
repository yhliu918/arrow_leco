#include <parquet/arrow/reader.h>
#include <fstream>
#include <iostream>
#include <sstream>

#include <gperftools/profiler.h>
#include <parquet/api/reader.h>
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
#include "stats.h"

using namespace arrow;

const int DATA_SIZE = 1024 * 1024 * 200;
const int BATCH_SIZE = DATA_SIZE;
const uint32_t ROW_GROUP_SIZE = 64 * 1024 * 1024;

std::string get_pq_name(parquet::Encoding::type encoding) {
  return "./encoding" + std::to_string(encoding) + "_rowgroup" +
         std::to_string(ROW_GROUP_SIZE) + "_datasize" + std::to_string(DATA_SIZE) +
         ".parquet";
}

arrow::Status encoder_decoder_test(parquet::Encoding::type encoding) {
  std::vector<int32_t> data;
  for (int32_t i = 0; i < DATA_SIZE; ++i) {
    data.push_back(i);
  }
  // ColumnDescriptor is only used for FIXED_LEN_BYTE_ARRAY
  auto encoder = parquet::MakeTypedEncoder<parquet::Int32Type>(
      encoding,
      /*use_dictionary=*/false, /*ColumnDescriptor* descr=*/nullptr);
  // seems that implement this func is enough for non-null case
  for (int32_t i = 0; i < DATA_SIZE / 1024; ++i) {
    encoder->Put(&data[i * 1024], 1024);
  }
  encoder->Put(&data[DATA_SIZE - DATA_SIZE % 1024], DATA_SIZE % 1024);
  auto buffer = encoder->FlushValues();
  std::vector<int32_t> decoded_data(DATA_SIZE);
  auto begin = stats::Time::now();
  auto decoder = parquet::MakeTypedDecoder<parquet::Int32Type>(
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

  auto schema = arrow::schema(
      {arrow::field("a", arrow::int32()), arrow::field("b", arrow::int32())});

  arrow::Int32Builder aBuilder;
  PARQUET_THROW_NOT_OK(aBuilder.AppendValues(a));

  arrow::Int32Builder bBuilder;
  PARQUET_THROW_NOT_OK(bBuilder.AppendValues(b));

  std::shared_ptr<arrow::Array> array_a, array_b;
  ARROW_ASSIGN_OR_RAISE(array_a, aBuilder.Finish());
  ARROW_ASSIGN_OR_RAISE(array_b, bBuilder.Finish());

  std::shared_ptr<arrow::Table> table = arrow::Table::Make(schema, {array_a, array_b});
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

arrow::Status data_gen(parquet::Encoding::type encoding, std::vector<int64_t>& a,
                       std::vector<int64_t>& b) {
  std::string parquet_name = get_pq_name(encoding);

  auto schema = arrow::schema(
      {arrow::field("a", arrow::int64()), arrow::field("b", arrow::int64())});

  arrow::Int64Builder aBuilder;
  PARQUET_THROW_NOT_OK(aBuilder.AppendValues(a));

  arrow::Int64Builder bBuilder;
  PARQUET_THROW_NOT_OK(bBuilder.AppendValues(b));

  std::shared_ptr<arrow::Array> array_a, array_b;
  ARROW_ASSIGN_OR_RAISE(array_a, aBuilder.Finish());
  ARROW_ASSIGN_OR_RAISE(array_b, bBuilder.Finish());

  std::shared_ptr<arrow::Table> table = arrow::Table::Make(schema, {array_a, array_b});
  std::shared_ptr<arrow::io::FileOutputStream> outfile;
  PARQUET_ASSIGN_OR_THROW(outfile, arrow::io::FileOutputStream::Open(parquet_name));

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
  PARQUET_THROW_NOT_OK(parquet::arrow::WriteTable(*table, arrow::default_memory_pool(),
                                                  outfile, row_group_size, properties));
  PARQUET_THROW_NOT_OK(outfile->Close());
  return arrow::Status::OK();
}

arrow::Status pure_scan(parquet::Encoding::type encoding) {
  auto begin = stats::Time::now();
  std::string parquet_name = get_pq_name(encoding);
  ARROW_ASSIGN_OR_RAISE(auto input, arrow::io::ReadableFile::Open(
                                        parquet_name, arrow::default_memory_pool()));

  // Instantiate TableReader from input stream and options
  std::unique_ptr<parquet::arrow::FileReader> pq_reader;

  // Read table from file
  auto reader_fut = parquet::ParquetFileReader::OpenAsync(input);
  ARROW_ASSIGN_OR_RAISE(std::unique_ptr<parquet::ParquetFileReader> pq_file_reader,
                        reader_fut.MoveResult());

  std::vector<int> columns;
  parquet::ScanFileContents(columns, BATCH_SIZE, pq_file_reader.get());
  stats::cout_sec(begin, "pure scan " + std::to_string(encoding));
  return arrow::Status::OK();
}

arrow::Status RunMain(int argc, char** argv) {
  std::string encoding = argv[1];
  // ARROW_RETURN_NOT_OK(encoder_decoder_test(parquet::Encoding::PLAIN));
  // ARROW_RETURN_NOT_OK(encoder_decoder_test(parquet::Encoding::FOR));
  // ARROW_RETURN_NOT_OK(encoder_decoder_test(parquet::Encoding::LECO));
  // ARROW_RETURN_NOT_OK(full_scan_test(parquet::Encoding::PLAIN));
  // ARROW_RETURN_NOT_OK(full_scan_test(parquet::Encoding::FOR));
  // ARROW_RETURN_NOT_OK(full_scan_test(parquet::Encoding::LECO));
  if (encoding == "PLAIN") {
    ARROW_RETURN_NOT_OK(pure_scan(parquet::Encoding::PLAIN));
  } else if (encoding == "FOR") {
    ARROW_RETURN_NOT_OK(pure_scan(parquet::Encoding::FOR));
  } else if (encoding == "LECO") {
    ARROW_RETURN_NOT_OK(pure_scan(parquet::Encoding::LECO));
  } else {
    std::cout << "wrong encoding" << std::endl;
  }
  system("cat /proc/$PPID/io");
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