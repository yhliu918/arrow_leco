#include <parquet/arrow/reader.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

// #include <gperftools/profiler.h>
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
#include "arrow/util/checked_cast.h"
#include "json.hpp"
#include "parquet/arrow/writer.h"
#include "parquet/file_reader.h"
#include "parquet/properties.h"
#include "stats.h"

namespace ds = arrow::dataset;
namespace fs = arrow::fs;
namespace cp = arrow::compute;

arrow::Status async_read_file(parquet::ParquetFileReader* reader) {
  arrow::io::IOContext io_context = arrow::io::default_io_context();
  arrow::io::CacheOptions cache_options = arrow::io::CacheOptions::Defaults();
  for (int i = 0; i < reader->metadata()->num_row_groups(); i++) {
    for (int j = 0; j < reader->metadata()->num_columns(); j++) {
      reader->PreBuffer({i}, {j}, io_context, cache_options);
      // reader->WhenBuffered({i}, {j}).Wait();
    }
  }
  return arrow::Status::OK();
}

arrow::Status RunMain(int argc, char** argv) {
  // std::ifstream ifs = std::ifstream(argv[1]);
  // nlohmann::json ex_jf = nlohmann::json::parse(ifs);
  // bool profiler_enabled = ex_jf["profiler_enabled"];
  // std::string file_path = ex_jf["file_path"];
  // std::string prof_name = ex_jf["prof_name"];
  // bool use_threads = ex_jf["use_threads"];
  // int batch_size = atoi(argv[2]);

  std::string file_path = argv[1];
  int async_read = atoi(argv[2]);
  // bool profiler_enabled = false;
  bool use_threads = false;
  int batch_size = 1024;
  std::string prof_name = "";
  ARROW_UNUSED(use_threads);
  // if (profiler_enabled) ProfilerStart(prof_name.c_str());
  // fs::S3FileSystem::

  ARROW_CHECK_OK(fs::InitializeS3(fs::S3GlobalOptions()));
  auto s3_options = fs::S3Options::Defaults();
  s3_options.region = "cn-north-1";
  ARROW_ASSIGN_OR_RAISE(auto fs, fs::S3FileSystem::Make(s3_options));
  // ARROW_ASSIGN_OR_RAISE(auto fs, fs::FileSystemFromUri(file_path));
  // auto s3fs = arrow::internal::checked_pointer_cast<fs::S3FileSystem>(fs);
  // s3fs->region
  ARROW_ASSIGN_OR_RAISE(auto input, fs->OpenInputFile(file_path));
  // fs::S3FileSystem::Make()

  // ARROW_ASSIGN_OR_RAISE(
  //     auto input, arrow::io::ReadableFile::Open(file_path,
  //     arrow::default_memory_pool()));

  // Instantiate TableReader from input stream and options
  std::unique_ptr<parquet::arrow::FileReader> pq_reader;

  // Read table from file
  auto begin = stats::Time::now();
  auto reader_fut = parquet::ParquetFileReader::OpenAsync(input);
  ARROW_ASSIGN_OR_RAISE(std::unique_ptr<parquet::ParquetFileReader> pq_file_reader,
                        reader_fut.MoveResult());
  std::thread th1;
  if (async_read) {
    th1 = std::thread(async_read_file, pq_file_reader.get());
  }
  // std::shared_ptr<parquet::FileMetaData> file_metadata = pq_file_reader->metadata();
  // int num_row_groups = file_metadata->num_row_groups();
  // std::cout << "num_row_groups: " << num_row_groups << std::endl;
  // Get the number of Columns
  std::vector<int> columns;
  parquet::ScanFileContents(columns, batch_size, pq_file_reader.get());
  stats::cout_sec(begin, "read pq");
  if (async_read) {
    th1.join();
  }
  // if (profiler_enabled) ProfilerStop();
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