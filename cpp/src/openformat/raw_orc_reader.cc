#include <fstream>
#include <iostream>
#include <sstream>

#include <gperftools/profiler.h>
#include <orc/OrcFile.hh>
// #include "arrow/status.h"
#include "json.hpp"
#include "stats.h"

int RunMain(int argc, char** argv) {
  std::ifstream ifs = std::ifstream(argv[1]);
  nlohmann::json ex_jf = nlohmann::json::parse(ifs);
  bool profiler_enabled = ex_jf["profiler_enabled"];
  std::string file_path = ex_jf["file_path"];
  std::string prof_name = ex_jf["prof_name"];
  // bool use_threads = ex_jf["use_threads"];
  // ARROW_UNUSED(use_threads);
  int batch_size = atoi(argv[2]);
  if (profiler_enabled) ProfilerStart(prof_name.c_str());

  // Read table from file
  auto begin = stats::Time::now();
  ORC_UNIQUE_PTR<orc::InputStream> inStream = orc::readLocalFile(file_path);
  orc::ReaderOptions options;
  ORC_UNIQUE_PTR<orc::Reader> reader = orc::createReader(std::move(inStream), options);
  orc::RowReaderOptions rowReaderOptions;
  ORC_UNIQUE_PTR<orc::RowReader> rowReader = reader->createRowReader(rowReaderOptions);
  ORC_UNIQUE_PTR<orc::ColumnVectorBatch> batch = rowReader->createRowBatch(batch_size);
  while (rowReader->next(*batch)) {
    // for (uint64_t r = 0; r < batch->numElements; ++r) {
    //   ... process row r from batch
    // }
  }
  stats::cout_sec(begin, "read orc");
  if (profiler_enabled) ProfilerStop();
  return 0;
}

int main(int argc, char** argv) {
  int status = RunMain(argc, argv);
  if (!status) {
    std::cerr << status << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}