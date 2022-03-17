#include <parquet/arrow/reader.h>

#include <iostream>

#include <gperftools/profiler.h>
#include "arrow/array.h"
#include "arrow/csv/api.h"
#include "arrow/io/file.h"
#include "stats.h"

arrow::Status RunMain(int argc, char** argv) {
  ProfilerStart("Generico_no_dict.prof");
  std::string file_name = "Generico_2";
  arrow::io::IOContext io_context = arrow::io::default_io_context();
  ARROW_ASSIGN_OR_RAISE(auto input, arrow::io::ReadableFile::Open(
                                        "/root/arrow-private/cpp/out/build/openformat/" +
                                            file_name + "_no_dict.parquet",
                                        arrow::default_memory_pool()));

  // Instantiate TableReader from input stream and options
  std::unique_ptr<parquet::arrow::FileReader> pq_reader;
  ARROW_RETURN_NOT_OK(
      parquet::arrow::OpenFile(input, arrow::default_memory_pool(), &pq_reader));

  pq_reader->set_use_threads(true);
  // Read table from ORC file
  auto begin = stats::Time::now();
  std::shared_ptr<arrow::Table> table;
  ARROW_RETURN_NOT_OK(pq_reader->ReadTable(&table));
  stats::cout_sec(begin, "read pq");
  ProfilerStop();
  // std::cout << table->schema()->ToString() << std::endl;
  // std::cout << orc_reader->GetCompression().ValueOrDie() << std::endl;
  // std::cout << orc_reader->NextStripeReader << std::endl;
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