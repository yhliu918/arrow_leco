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
  std::vector<int> columns;
  parquet::ScanFileContents(columns, batch_size, pq_file_reader.get());
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