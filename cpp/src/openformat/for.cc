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
#include "parquet/encoding.h"
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

arrow::Status RunMain(int argc, char** argv) {
  ARROW_RETURN_NOT_OK(encoder_decoder_test(parquet::Encoding::PLAIN));
  ARROW_RETURN_NOT_OK(encoder_decoder_test(parquet::Encoding::FOR));
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