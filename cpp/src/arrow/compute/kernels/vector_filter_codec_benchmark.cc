// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include <cstdint>
#include <sstream>
#include <fstream>
#include <sys/time.h>

#include "arrow/compute/api_vector.h"
#include "arrow/compute/kernels/test_util.h"
#include "arrow/testing/gtest_util.h"
#include "arrow/testing/random.h"

namespace arrow {
namespace compute {

constexpr auto kSeed = 0x0ff1ce;

// The benchmark state parameter references this vector of cases. Test high and
// low selectivity filters.

template <typename T>
static std::vector<T> load_data_binary(const std::string& filename,
    bool print = true) {
    std::vector<T> data;

    std::ifstream in(filename, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "unable to open " << filename << std::endl;
        exit(EXIT_FAILURE);
    }
    // Read size.
    uint64_t size;
    in.read(reinterpret_cast<char*>(&size), sizeof(uint64_t));
    data.resize(size);
    // Read values.
    in.read(reinterpret_cast<char*>(data.data()), size * sizeof(T));
    in.close();

    return data;
}

template <typename T>
static std::vector<T> load_data(const std::string& filename) {
    std::vector<T> data;
    std::ifstream srcFile(filename, std::ios::in);
    if (!srcFile) {
        std::cout << "error opening source file." << std::endl;
        return data;
    }

    while (srcFile.good()) {
        T next;
        srcFile >> next;
        if (!srcFile.good()) { break; }
        data.emplace_back(next);

    }
    srcFile.close();

    return data;
}

static std::vector<uint8_t> load_bitmap(const std::string& filename) {
    std::vector<uint8_t> data;
    std::ifstream srcFile(filename, std::ios::in);
    if (!srcFile) {
        std::cout << "error opening bitmap file." << std::endl;
    }

    while (srcFile.good()) {
        int next;
        srcFile >> next;
        if (!srcFile.good()) { break; }
        if(next){
          data.emplace_back(0);
        }
        else{
          data.emplace_back(1);
        }
    }
    srcFile.close();

    return data;
}

double getNow() {
  struct timeval tv;
  gettimeofday(&tv, 0);
  return tv.tv_sec + tv.tv_usec / 1000000.0;
}


void Filterbench(std::string file_path, int blocks, std::string bitmap_file, CODEC codec = CODEC::PLAIN) {
    std::vector<int64_t> data = load_data<int64_t>(file_path);
    Int64Builder builder;
    builder.SetCompress(codec);
    builder.AppendValues(data.data(), data.size(), nullptr, blocks);
    std::shared_ptr<Array> out;
    builder.Finish(&out);

    std::vector<uint8_t> bitmap = load_bitmap(bitmap_file);
    BooleanBuilder builder_bitmap;
    for(auto item: bitmap){
      builder_bitmap.Append(bool(item));
    }
    std::shared_ptr<BooleanArray> filter;
    builder_bitmap.Finish(&filter);

    double start = getNow();              
    auto result = Filter(out, filter);
    const int64_t* result_sq = result->array()->GetValues<int64_t>(1);
    std::cout<<result_sq[0]<<std::endl;
    double end = getNow();
    std::cout<<file_path<<" "<<bitmap_file<<" "<<codec<<" "<<(end - start)<<std::endl;
  }


}  // namespace compute
}  // namespace arrow

int main(int argc, const char* argv[]){
  std::string file_path = argv[1];
  std::string bitmap_file = argv[2];
  int blocks = atoi(argv[3]);
  int codec_num = atoi(argv[4]);
  

  arrow::CODEC codec = arrow::CODEC::PLAIN;
  switch (codec_num)
  {
  case 1:
    codec = arrow::CODEC::PLAIN;
    break;
  case 2:
    codec = arrow::CODEC::FOR;
    break;
  case 3:
    codec = arrow::CODEC::LECO;
    break;
  case 4:
    codec = arrow::CODEC::DELTA;
    break;
  default:
    break;
  }
  
  arrow::compute::Filterbench("/root/arrow-private/cpp/Learn-to-Compress/data/"+file_path, blocks, "/root/arrow-private/cpp/Learn-to-Compress/data/bitmap_random/"+bitmap_file, codec);
}