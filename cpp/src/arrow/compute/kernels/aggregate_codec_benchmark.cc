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


#include <vector>
#include <sys/time.h>
#include "arrow/array/array_primitive.h"
#include "arrow/compute/api.h"
#include "arrow/compute/exec/aggregate.h"
#include "arrow/testing/gtest_util.h"
#include "arrow/testing/random.h"
#include "arrow/util/bit_util.h"
#include "arrow/util/bitmap_reader.h"
#include "arrow/compute/api_vector.h"
#include "arrow/compute/kernels/test_util.h"

namespace arrow {
namespace compute {

#include <cassert>
#include <cmath>
#include <iostream>
#include <random>

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
template <typename ArrowType>
static void SumKernel( CODEC codecin, std::string file_path, int blocks, bool binary, std::string bitmap_file = "") {
  std::vector<int64_t> data;
  if(binary){
    data = load_data_binary<int64_t>(file_path);
  }
  else{
    data = load_data<int64_t>(file_path);
  }
  std::vector<uint8_t> bitmap;
  if(bitmap_file.size()>0){
    bitmap = load_bitmap(bitmap_file);
  }
  // for(auto i : bitmap){
  //   if(i == 1){
  //     std::cout<<unsigned(i)<<std::endl;
  //   }
  // }
  
  arrow::Int64Builder builder;
  builder.SetCompress(codecin);
  builder.AppendValues(data.data(), data.size(), bitmap.data(), blocks);
  std::shared_ptr<Array> out;
  builder.Finish(&out);
  // auto rand = random::RandomArrayGenerator(1923);
  // auto out = rand.Numeric<ArrowType>(data.size(), -100, 100, 0.01);
  double start = getNow();              
  Sum(out);
  double end = getNow();
  std::cout<<file_path<<" "<<bitmap_file<<" "<<codecin<<" "<<(end - start)<<std::endl;
}

}  // namespace compute
}  // namespace arrow

int main(int argc, const char* argv[]){
  std::string file_path = argv[1];
  int blocks = atoi(argv[2]);
  int codec_num = atoi(argv[3]);
  bool binary = atoi(argv[4]);
  std::string bitmap_path = "";
  if(argc>5){
    bitmap_path ="/root/arrow-private/cpp/Learn-to-Compress/data/bitmap_random/"+std::string(argv[5]);
  }

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
  
  arrow::compute::SumKernel<arrow::Int64Type>(codec, "/root/arrow-private/cpp/Learn-to-Compress/data/"+file_path, blocks, binary, bitmap_path);
}