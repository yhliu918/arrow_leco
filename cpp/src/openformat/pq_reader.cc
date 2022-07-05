#include <parquet/arrow/reader.h>

#include <fstream>
#include <iostream>
#include <sstream>

#include <gperftools/profiler.h>
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

#define ABORT_ON_FAILURE(expr)                     \
  do {                                             \
    arrow::Status status_ = (expr);                \
    if (!status_.ok()) {                           \
      std::cerr << status_.message() << std::endl; \
      abort();                                     \
    }                                              \
  } while (0);

// Read a dataset, but select only column "b" and only rows where b < 4.
//
// This is useful when you only want a few columns from a dataset. Where possible,
// Datasets will push down the column selection such that less work is done.
std::shared_ptr<arrow::Table> FilterAndSelectDataset(
    const std::shared_ptr<fs::FileSystem>& filesystem,
    const std::shared_ptr<ds::FileFormat>& format, const std::string& base_dir,
    const int filter_on, const int proj_on) {
  fs::FileSelector selector;
  selector.base_dir = base_dir;
  auto factory = ds::FileSystemDatasetFactory::Make(filesystem, selector, format,
                                                    ds::FileSystemFactoryOptions())
                     .ValueOrDie();
  auto dataset = factory->Finish().ValueOrDie();
  // Read specified columns with a row filter
  auto scan_builder = dataset->NewScan().ValueOrDie();
  if (proj_on) {
    ABORT_ON_FAILURE(
        scan_builder->Project({"l_quantity", "l_extendedprice", "l_discount"}));
  }

  if (filter_on) {
    auto begin = stats::Time::now();
    ABORT_ON_FAILURE(
        scan_builder->Filter(cp::less(cp::field_ref("l_quantity"), cp::literal(24))));
    stats::cout_sec(begin, "bind expression");
    // ABORT_ON_FAILURE(scan_builder->Filter(cp::and_(
    //     cp::less(cp::field_ref("l_shipdate"), cp::literal("1995-01-01")),
    //     cp::greater_equal(cp::field_ref("l_shipdate"), cp::literal("1994-01-01")))));
  }
  auto scanner = scan_builder->Finish().ValueOrDie();
  return scanner->ToTable().ValueOrDie();
}

arrow::Status test_buff_builder(std::shared_ptr<arrow::Table> table) {
  std::cout << table->num_rows() << " rows" << std::endl;
  arrow::BinaryBuilder builder;
  auto array = std::static_pointer_cast<arrow::StringArray>(
      table->GetColumnByName("Anunciante")->chunk(0));
  std::cout << array->length() << " array len" << std::endl;
  auto begin = stats::Time::now();
  for (int i = 0; i < array->length(); i++) {
    builder.Append(array->Value(i));
  }
  stats::cout_sec(begin, "append time");
  return arrow::Status::OK();
}

arrow::Status RunMain(int argc, char** argv) {
  std::ifstream ifs = std::ifstream(argv[1]);
  nlohmann::json ex_jf = nlohmann::json::parse(ifs);
  bool profiler_enabled = ex_jf["profiler_enabled"];
  std::string file_path = ex_jf["file_path"];
  std::string prof_name = ex_jf["prof_name"];
  bool use_threads = ex_jf["use_threads"];
  if (profiler_enabled) ProfilerStart(prof_name.c_str());
  ARROW_ASSIGN_OR_RAISE(
      auto input, arrow::io::ReadableFile::Open(file_path, arrow::default_memory_pool()));
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
  std::unique_ptr<parquet::arrow::FileReader> pq_reader;
  // ARROW_RETURN_NOT_OK(
  //     parquet::arrow::OpenFile(input, arrow::default_memory_pool(), &pq_reader));
  auto pq_file_reader = parquet::ParquetFileReader::Open(input);
  parquet::ArrowReaderProperties arrow_properties;
  arrow_properties.set_read_dictionary(0, true);
  parquet::arrow::FileReader::Make(arrow::default_memory_pool(),
                                   std::move(pq_file_reader), arrow_properties,
                                   &pq_reader);

  pq_reader->set_use_threads(use_threads);
  // Read table from ORC file
  auto begin = stats::Time::now();
  std::shared_ptr<arrow::Table> table;
  ARROW_RETURN_NOT_OK(pq_reader->ReadTable(&table));
  stats::cout_sec(begin, "read pq");
  if (profiler_enabled) ProfilerStop();
  // test_buff_builder(table);
  // std::cout << table->schema()->ToString() << std::endl;
  // std::cout << orc_reader->GetCompression().ValueOrDie() << std::endl;
  // std::cout << orc_reader->NextStripeReader << std::endl;
  return arrow::Status::OK();
}

int main(int argc, char** argv) {
  // int filter_on = atoi(argv[1]);
  // int proj_on = atoi(argv[2]);
  // std::string base_path = argv[3];
  // std::string root_path;
  // auto fs = std::make_shared<fs::LocalFileSystem>(fs::LocalFileSystem());
  // auto format = std::make_shared<ds::ParquetFileFormat>();
  // auto begin = stats::Time::now();
  // // ProfilerStart("profile.out");
  // auto table = FilterAndSelectDataset(fs, format, base_path, filter_on, proj_on);
  // table->num_rows();
  // // ProfilerStop();
  // stats::cout_sec(begin, "read pq");

  // std::shared_ptr<arrow::io::FileOutputStream> outfile;
  // std::string file_name = "lineitem_1K";
  // PARQUET_ASSIGN_OR_THROW(
  //     outfile, arrow::io::FileOutputStream::Open("/root/arrow-private/cpp/" + file_name
  //     +
  //                                                ".parquet"));
  // // FIXME: hard code for now
  // uint32_t row_group_size = 1 * 1024 * 1024;
  // uint32_t dictionary_pages_size = 1 * 1024 * 1024;  // 1MB
  // arrow::Compression::type codec = arrow::Compression::SNAPPY;
  // std::shared_ptr<parquet::WriterProperties> properties =
  //     parquet::WriterProperties::Builder()
  //         .dictionary_pagesize_limit(dictionary_pages_size)
  //         ->compression(codec)
  //         ->enable_dictionary()
  //         ->build();

  // begin = stats::Time::now();
  // PARQUET_THROW_NOT_OK(parquet::arrow::WriteTable(*table, arrow::default_memory_pool(),
  //                                                 outfile, row_group_size,
  //                                                 properties));
  arrow::Status status = RunMain(argc, argv);
  if (!status.ok()) {
    std::cerr << status << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}