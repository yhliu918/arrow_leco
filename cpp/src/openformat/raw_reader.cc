#include <parquet/arrow/reader.h>

#include <fstream>
#include <iostream>
#include <sstream>

#include <gperftools/profiler.h>
#include <parquet/api/reader.h>
#include "arrow/array.h"
#include "arrow/array/builder_binary.h"
#include "arrow/buffer_builder.h"
#include "arrow/compute/api_aggregate.h"
#include "arrow/compute/kernels/util_internal.h"
#include "arrow/csv/api.h"
#include "arrow/dataset/api.h"
#include "arrow/dataset/discovery.h"
#include "arrow/dataset/file_base.h"
#include "arrow/filesystem/api.h"
#include "arrow/io/file.h"
#include "json.hpp"
#include "parquet/arrow/writer.h"
#include "parquet/file_reader.h"
#include "parquet/properties.h"
#include "stats.h"

namespace ds = arrow::dataset;
namespace fs = arrow::fs;
namespace cp = arrow::compute;

arrow::Status RunMain(int argc, char** argv) {
  std::ifstream ifs = std::ifstream(argv[1]);
  nlohmann::json ex_jf = nlohmann::json::parse(ifs);
  bool profiler_enabled = ex_jf["profiler_enabled"];
  std::string file_path = ex_jf["file_path"];
  std::string prof_name = ex_jf["prof_name"];
  bool use_threads = ex_jf["use_threads"];
  int batch_size = atoi(argv[2]);
  ARROW_UNUSED(use_threads);
  if (profiler_enabled) ProfilerStart(prof_name.c_str());
  ARROW_ASSIGN_OR_RAISE(
      auto input, arrow::io::ReadableFile::Open(file_path, arrow::default_memory_pool()));

  // Instantiate TableReader from input stream and options
  std::unique_ptr<parquet::arrow::FileReader> pq_reader;

  // Read table from file
  auto begin = stats::Time::now();
  auto pq_file_reader = parquet::ParquetFileReader::Open(input);
  std::shared_ptr<parquet::FileMetaData> file_metadata = pq_file_reader->metadata();

  int num_row_groups = file_metadata->num_row_groups();
  std::cout << "num_row_groups: " << num_row_groups << std::endl;
  // Get the number of Columns
  int num_columns = file_metadata->num_columns();
  assert(num_columns == 1);
  uint64_t read_cnt = 0;
  for (int r = 0; r < num_row_groups; ++r) {
    // Get the RowGroup Reader
    std::shared_ptr<parquet::RowGroupReader> row_group_reader =
        pq_file_reader->RowGroup(r);

    // assert(row_group_reader->metadata()->total_byte_size() < ROW_GROUP_SIZE);

    int64_t values_read = 0;
    int64_t rows_read = 0;
    int16_t definition_level;
    int16_t repetition_level;
    std::shared_ptr<parquet::ColumnReader> column_reader;

    ARROW_UNUSED(rows_read);  // prevent warning in release build
    ARROW_UNUSED(values_read);
    ARROW_UNUSED(definition_level);
    ARROW_UNUSED(repetition_level);
    for (int col_id = 0; col_id < num_columns; ++col_id) {
      // Get the Column Reader for the ByteArray column
      column_reader = row_group_reader->Column(col_id);
      auto scanner = parquet::Scanner::Make(column_reader, batch_size);
      bool is_null;
      parquet::BoolScanner* b_scanner;
      parquet::Int32Scanner* i32_scanner;
      parquet::Int64Scanner* i64_scanner;
      parquet::Int96Scanner* i96_scanner;
      parquet::FloatScanner* f_scanner;
      parquet::DoubleScanner* d_scanner;
      parquet::ByteArrayScanner* ba_scanner;
      parquet::FixedLenByteArrayScanner* flba_scanner;
      bool value;
      int32_t vi32;
      int64_t vi64;
      parquet::Int96 vi96;
      float vf;
      double vd;
      parquet::ByteArray vba;
      parquet::FixedLenByteArray vfldba;
      switch (column_reader->type()) {
        case parquet::Type::BOOLEAN:
          b_scanner = static_cast<parquet::BoolScanner*>(scanner.get());
          while (b_scanner->NextValue(&value, &is_null)) {
            assert(true);
            read_cnt++;
            // printf("%.*s\n", value.len, value.ptr);
          }
          break;
        case parquet::Type::INT32:
          i32_scanner = static_cast<parquet::Int32Scanner*>(scanner.get());
          while (i32_scanner->NextValue(&vi32, &is_null)) {
            assert(true);
            read_cnt++;
            // printf("%.*s\n", value.len, value.ptr);
          }
          break;
        case parquet::Type::INT64:
          i64_scanner = static_cast<parquet::Int64Scanner*>(scanner.get());
          while (i64_scanner->NextValue(&vi64, &is_null)) {
            assert(true);
            read_cnt++;
            // printf("%.*s\n", value.len, value.ptr);
          }
          break;
        case parquet::Type::INT96:
          i96_scanner = static_cast<parquet::Int96Scanner*>(scanner.get());
          while (i96_scanner->NextValue(&vi96, &is_null)) {
            assert(true);
            read_cnt++;
            // printf("%.*s\n", value.len, value.ptr);
          }
          break;
        case parquet::Type::FLOAT:
          f_scanner = static_cast<parquet::FloatScanner*>(scanner.get());
          while (f_scanner->NextValue(&vf, &is_null)) {
            assert(true);
            read_cnt++;
            // printf("%.*s\n", value.len, value.ptr);
          }
          break;
        case parquet::Type::DOUBLE:
          d_scanner = static_cast<parquet::DoubleScanner*>(scanner.get());
          while (d_scanner->NextValue(&vd, &is_null)) {
            assert(true);
            read_cnt++;
            // printf("%.*s\n", value.len, value.ptr);
          }
          break;
        case parquet::Type::BYTE_ARRAY:
          ba_scanner = static_cast<parquet::ByteArrayScanner*>(scanner.get());
          while (ba_scanner->NextValue(&vba, &is_null)) {
            assert(true);
            read_cnt++;
            // printf("%.*s\n", value.len, value.ptr);
          }
          break;
        case parquet::Type::FIXED_LEN_BYTE_ARRAY:
          flba_scanner = static_cast<parquet::FixedLenByteArrayScanner*>(scanner.get());
          while (flba_scanner->NextValue(&vfldba, &is_null)) {
            assert(true);
            read_cnt++;
            // printf("%.*s\n", value.len, value.ptr);
          }
          break;
        default:
          parquet::ParquetException::NYI("type reader not implemented");
      }
    }
  }
  assert(read_cnt == file_metadata->num_rows() * num_columns);
  stats::cout_sec(begin, "read pq");
  if (profiler_enabled) ProfilerStop();
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