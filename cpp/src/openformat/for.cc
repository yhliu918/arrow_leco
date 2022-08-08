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

const int DATA_SIZE = 10;

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
  encoder->Put(&data[0], DATA_SIZE);
  auto buffer = encoder->FlushValues();
  auto decoder = parquet::MakeTypedDecoder<parquet::Int32Type>(
      encoding, /*ColumnDescriptor* descr=*/nullptr);
  int32_t decoded_data[DATA_SIZE];
  decoder->SetData(DATA_SIZE, buffer->data(), static_cast<int>(buffer->size()));
  decoder->Decode(decoded_data, DATA_SIZE);
  for (size_t i = 0; i < DATA_SIZE; ++i) {
    if (data[i] != decoded_data[i]) {
      std::cout << data[i] << " " << decoded_data[i] << std::endl;
    }
  }
  return arrow::Status::OK();
}

arrow::Status full_scan_test(parquet::Encoding::type encoding) {
  const std::vector<int32_t> a = {1, 2, 3, 4, 5};
  const std::vector<int64_t> b = {5, 4, 3, 2, 1};

  auto schema = arrow::schema(
      {arrow::field("a", arrow::int32()), arrow::field("b", arrow::int64())});

  arrow::Int32Builder aBuilder;
  PARQUET_THROW_NOT_OK(aBuilder.AppendValues(a));

  arrow::Int64Builder bBuilder;
  PARQUET_THROW_NOT_OK(bBuilder.AppendValues(b));

  std::shared_ptr<arrow::Array> array_a, array_b;
  ARROW_ASSIGN_OR_RAISE(array_a, aBuilder.Finish());
  ARROW_ASSIGN_OR_RAISE(array_b, bBuilder.Finish());

  std::shared_ptr<arrow::Table> table = arrow::Table::Make(schema, {array_a, array_b});
  std::shared_ptr<arrow::io::FileOutputStream> outfile;
  PARQUET_ASSIGN_OR_THROW(outfile,
                          arrow::io::FileOutputStream::Open("/tmp/test.parquet"));
  // FIXME: hard code for now
  uint32_t row_group_size = 1 * 1024 * 1024;         // 64M / 10
  uint32_t dictionary_pages_size = 1 * 1024 * 1024;  // 64M * 0.03
  arrow::Compression::type codec = arrow::Compression::UNCOMPRESSED;
  std::shared_ptr<parquet::WriterProperties> properties =
      parquet::WriterProperties::Builder()
          .dictionary_pagesize_limit(dictionary_pages_size)
          ->compression(codec)
          ->disable_dictionary()
          ->encoding(encoding)
          ->build();
  PARQUET_THROW_NOT_OK(parquet::arrow::WriteTable(*table, arrow::default_memory_pool(),
                                                  outfile, row_group_size, properties));
  PARQUET_THROW_NOT_OK(outfile->Close());
  ARROW_ASSIGN_OR_RAISE(
      auto input,
      arrow::io::ReadableFile::Open("/tmp/test.parquet", arrow::default_memory_pool()));

  // Instantiate TableReader from input stream and options
  std::unique_ptr<parquet::arrow::FileReader> pq_reader;

  // Read table from file
  auto reader_fut = parquet::ParquetFileReader::OpenAsync(input);
  ARROW_ASSIGN_OR_RAISE(std::unique_ptr<parquet::ParquetFileReader> pq_file_reader,
                        reader_fut.MoveResult());

  // std::shared_ptr<parquet::FileMetaData> file_metadata = pq_file_reader->metadata();
  // int num_row_groups = file_metadata->num_row_groups();
  // std::cout << "num_row_groups: " << num_row_groups << std::endl;
  // Get the number of Columns
  std::vector<int> columns;
  int batch_size = 1024;
  parquet::ScanFileContents(columns, batch_size, pq_file_reader.get());
  return arrow::Status::OK();
}

arrow::Status RunMain(int argc, char** argv) {
  ARROW_RETURN_NOT_OK(encoder_decoder_test(parquet::Encoding::PLAIN));
  ARROW_RETURN_NOT_OK(encoder_decoder_test(parquet::Encoding::FOR));
  ARROW_RETURN_NOT_OK(full_scan_test(parquet::Encoding::PLAIN));
  ARROW_RETURN_NOT_OK(full_scan_test(parquet::Encoding::FOR));
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