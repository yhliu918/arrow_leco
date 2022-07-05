#include <iostream>

#include <parquet/arrow/reader.h>
#include "arrow/adapters/orc/adapter.h"
#include "arrow/adapters/orc/options.h"
#include "arrow/array.h"
#include "arrow/csv/api.h"
#include "arrow/io/file.h"
#include "arrow/util/key_value_metadata.h"
#include "stats.h"

arrow::Status RunMain(int argc, char** argv) {
  std::string pq_path = argv[1];
  std::string orc_path = argv[2];
  // bool use_threads = true;
  ARROW_ASSIGN_OR_RAISE(
      auto input, arrow::io::ReadableFile::Open(pq_path, arrow::default_memory_pool()));
  ARROW_ASSIGN_OR_RAISE(auto orc_input, arrow::io::ReadableFile::Open(
                                            orc_path, arrow::default_memory_pool()));
  // ARROW_ASSIGN_OR_RAISE(
  //     auto input,
  //     arrow::io::ReadableFile::Open(
  //         "/root/arrow-private/cpp/submodules/OpenFormat/python/orders.parquet",
  //         arrow::default_memory_pool()));
  // ARROW_ASSIGN_OR_RAISE(auto input, arrow::io::ReadableFile::Open(
  //                                       "/root/arrow-private/cpp/out/build/openformat/"
  //                                       +
  //                                           file_name + "_both_no.parquet",
  //                                       arrow::default_memory_pool()));

  // Instantiate TableReader from input stream and options
  // std::unique_ptr<parquet::arrow::FileReader> pq_reader;
  // ARROW_RETURN_NOT_OK(
  //     parquet::arrow::OpenFile(input, arrow::default_memory_pool(), &pq_reader));

  // pq_reader->set_use_threads(use_threads);
  // // Read table from ORC file
  // auto begin = stats::Time::now();
  // std::shared_ptr<arrow::Table> table;
  // ARROW_RETURN_NOT_OK(pq_reader->ReadTable(&table));
  // stats::cout_sec(begin, "read pq");

  // // Write Arrow Table into ORC format
  // ARROW_ASSIGN_OR_RAISE(auto outstream,
  //                       arrow::io::FileOutputStream::Open("./lineitem_1M.orc"));
  // arrow::adapters::orc::WriteOptions write_options;
  // write_options.compression = arrow::Compression::SNAPPY;
  // write_options.dictionary_key_size_threshold = 0.8;
  // // write_options.stripe_size = 1048576;

  // ARROW_ASSIGN_OR_RAISE(auto orc_writer, arrow::adapters::orc::ORCFileWriter::Open(
  //                                            outstream.get(), write_options));
  // begin = stats::Time::now();
  // ARROW_RETURN_NOT_OK(orc_writer->Write(*table));
  // stats::cout_sec(begin, "Write orc");

  // Instantiate TableReader from input stream and options
  ARROW_ASSIGN_OR_RAISE(auto orc_reader, arrow::adapters::orc::ORCFileReader::Open(
                                             orc_input, arrow::default_memory_pool()))

  // Read table from ORC file
  auto begin = stats::Time::now();
  ARROW_ASSIGN_OR_RAISE(auto table, orc_reader->Read());
  stats::cout_sec(begin, "read orc");

  std::cout << table->schema()->ToString() << std::endl;
  std::cout << orc_reader->GetCompression().ValueOrDie() << std::endl;
  auto kv_meta = orc_reader->ReadMetadata().ValueOrDie();
  for (int64_t i = 0; i < kv_meta->size(); i++) {
    std::cout << kv_meta->key(i) << ": " << kv_meta->value(i) << std::endl;
  }
  // std::cout << orc_reader->ReadMetadata() << std::endl;
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