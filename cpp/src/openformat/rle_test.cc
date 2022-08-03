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
#include "arrow/util/rle_encoding.h"
#include "json.hpp"
#include "parquet/arrow/writer.h"
#include "parquet/file_reader.h"
#include "parquet/properties.h"
#include "stats.h"

namespace ds = arrow::dataset;
namespace fs = arrow::fs;
namespace cp = arrow::compute;

const size_t DEFAULT_MEM_STREAM_SIZE = 10 * 1024 * 1024;  // 10MB
const size_t ROW_GROUP_SIZE = 1 * 1024 * 1024;
#define BATCH_SIZE 1024
#define GLOBAL_CAR 6122
#define LOCAL_CAR 1454

arrow::Status RunMain(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <file>" << std::endl;
    return arrow::Status::OK();
  }
  std::string source_file = std::string(argv[1]);
  std::vector<int64_t> data;
  std::ifstream srcFile(source_file, std::ios::in);
  if (!srcFile) {
    std::cout << "error opening source file." << std::endl;
    return arrow::Status::OK();
  }
  while (1) {
    int64_t next;
    srcFile >> next;
    if (srcFile.eof()) {
      break;
    }
    data.push_back(next);
  }
  srcFile.close();
  std::cout << "data size: " << data.size() << std::endl;
  size_t num_buffers = data.size() / ROW_GROUP_SIZE;  // only encode 19 * 1M rows now
  std::cout << "num buffers: " << num_buffers << std::endl;
  std::vector<uint8_t> buffers[num_buffers];
  for (size_t i = 0; i < num_buffers; i++) {
    buffers[i].resize(DEFAULT_MEM_STREAM_SIZE);
  }
  size_t data_idx = 0;
  size_t encoded_size = 0;
  for (size_t i = 0; i < num_buffers; i++) {
    arrow::util::RleEncoder encoder(buffers[i].data(), DEFAULT_MEM_STREAM_SIZE,
                                    arrow::bit_util::Log2(LOCAL_CAR));
    for (; data_idx < data.size() && data_idx < ROW_GROUP_SIZE * (i + 1); ++data_idx) {
      encoder.Put(data[data_idx]);
    }
    encoder.Flush();
    encoded_size += encoder.len();
  }
  std::cout << "encoded size: " << encoded_size << std::endl;
  std::vector<int32_t> decoded_data;
  decoded_data.resize(data.size());
  int32_t* batch = decoded_data.data();
  double decode_time = 0;
  data_idx = 0;
  for (size_t i = 0; i < num_buffers; ++i) {
    arrow::util::RleDecoder decoder(buffers[i].data(), DEFAULT_MEM_STREAM_SIZE,
                                    arrow::bit_util::Log2(LOCAL_CAR));
    int32_t* start_tmp = batch;
    auto begin = stats::Time::now();
    for (size_t i = 0; i < ROW_GROUP_SIZE / BATCH_SIZE; i++) {
      decoder.GetBatch(batch, BATCH_SIZE);
      batch += BATCH_SIZE;
    }
    // decoder.GetBatch(batch, ROW_GROUP_SIZE % BATCH_SIZE);
    // batch += ROW_GROUP_SIZE % BATCH_SIZE;
    decode_time += ((stats::fsec)(stats::Time::now() - begin)).count();
    for (size_t i = 0; i < ROW_GROUP_SIZE; i++) {
      if (start_tmp[i] != data[data_idx]) {
        std::cout << "error: " << batch[i] << " != " << data[data_idx] << std::endl;
      }
      data_idx++;
    }
  }
  // arrow::util::RleDecoder decoder(buffers[num_buffers - 1].data(),
  //                                 DEFAULT_MEM_STREAM_SIZE,
  //                                 arrow::bit_util::Log2(GLOBAL_CAR));
  // int32_t* start = batch;
  // auto begin = stats::Time::now();
  // for (size_t i = 0; i < (data.size() % ROW_GROUP_SIZE) / BATCH_SIZE; i++) {
  //   decoder.GetBatch(batch, BATCH_SIZE);
  //   batch += BATCH_SIZE;
  // }
  // decoder.GetBatch(batch, (data.size() % ROW_GROUP_SIZE) % BATCH_SIZE);
  // batch += (data.size() % ROW_GROUP_SIZE) % BATCH_SIZE;
  // decode_time += ((stats::fsec)(stats::Time::now() - begin)).count();
  // for (size_t i = 0; i < data.size() % ROW_GROUP_SIZE; i++) {
  //   if (start[i] != data[data_idx++]) {
  //     std::cout << "error" << std::endl;
  //   }
  // }
  if (data_idx != ROW_GROUP_SIZE * num_buffers) {
    std::cout << "error, how many got:" << data_idx << std::endl;
  }
  std::cout << "decode time: " << decode_time << std::endl;
  std::cout << "rows encoded: " << data_idx << std::endl;
  // stats::cout_sec(begin, "RLE decode");

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