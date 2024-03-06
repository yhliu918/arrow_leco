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
using arrow::compute::internal::GroupBy;
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
        if(!next){
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
static void SumKernel( CODEC codecin, std::string file_path, int blocks, bool binary, std::string source_two, std::string bitmapfile) {
  std::vector<int64_t> data;
  if(binary){
    data = load_data_binary<int64_t>(file_path);
  }
  else{
    data = load_data<int64_t>(file_path);
  }
  std::vector<int64_t> data2 = load_data<int64_t>(source_two);


    // std::vector<uint8_t> bitmap = load_bitmap(bitmapfile);
    // bitmap.resize(data2.size());
    std::vector<uint8_t> bitmap = load_bitmap(bitmapfile);
    BooleanBuilder builder_bitmap;
    for(auto item: bitmap){
      builder_bitmap.Append(bool(item));
    }
    std::shared_ptr<BooleanArray> filter;
    builder_bitmap.Finish(&filter);

  
  // arrow::Int64Builder builder;
  // builder.SetCompress(codecin);
  // builder.AppendValues(data.data(), data2.size());
  // std::shared_ptr<Array> col1;
  // builder.Finish(&col1);

  // arrow::Int64Builder builder2;
  // builder2.SetCompress(codecin);
  // builder2.AppendValues(data2.data(), data2.size());
  // std::shared_ptr<Array> col2;
  // builder2.Finish(&col2);

  // double start = getNow();          
  // auto col1_new = Filter(col1, filter)->array();
  // auto col2_new = Filter(col2, filter)->array();
  // // auto rand = random::RandomArrayGenerator(1923);
  // // auto out = rand.Numeric<ArrowType>(data.size(), -100, 100, 0.01);

      
  // std::vector<Aggregate> aggregates;
  // aggregates.reserve(1);


  //   aggregates.push_back({"hash_sum", std::move(nullptr),
  //                         "agg_" + std::to_string(0),"hash_sum"});
  

  //   auto result = GroupBy({col1_new}, {col2_new}, aggregates);
  //   // std::cout<< result->array()->GetValues<int64_t>(1)[0]<<std::endl;

  
  // double end = getNow();


  arrow::Int64Builder builder;
  builder.SetCompress(codecin);
  builder.AppendValues(data.data(), data2.size(), bitmap.data());
  std::shared_ptr<Array> col1;
  builder.Finish(&col1);

  arrow::Int64Builder builder2;
  builder2.SetCompress(codecin);
  builder2.AppendValues(data2.data(), data2.size(), bitmap.data());
  std::shared_ptr<Array> col2;
  builder2.Finish(&col2);

  double start = getNow();          
  std::vector<Aggregate> aggregates;
  aggregates.reserve(1);


    aggregates.push_back({"hash_sum", std::move(nullptr),
                          "agg_" + std::to_string(0),"hash_sum"});
  

    auto result = GroupBy({col1}, {col2}, aggregates);
    // std::cout<< result->array()->GetValues<int64_t>(1)[0]<<std::endl;

  
  double end = getNow();
  std::cout<<file_path<<" "<<source_two<<" "<<codecin<<" "<<(end - start)<<std::endl;
}

}  // namespace compute
}  // namespace arrow

int main(int argc, const char* argv[]){
  std::string file_path = argv[1];
  int blocks = atoi(argv[2]);
  int codec_num = atoi(argv[3]);
  bool binary = atoi(argv[4]);
  std::string source2 = argv[5];
  std::string bitmap = argv[6];

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
  
  arrow::compute::SumKernel<arrow::Int64Type>(codec, "/root/arrow-private/cpp/Learn-to-Compress/data/"+file_path, blocks, binary,  "/root/arrow-private/cpp/Learn-to-Compress/data/"+ source2, "/root/arrow-private/cpp/Learn-to-Compress/data/bitmap_random/"+ bitmap);
}