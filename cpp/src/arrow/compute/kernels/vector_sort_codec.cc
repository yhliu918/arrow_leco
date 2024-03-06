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


#include "arrow/compute/api_vector.h"
#include "arrow/compute/kernels/test_util.h"
#include "arrow/table.h"
#include "arrow/testing/gtest_util.h"
#include "arrow/testing/random.h"
#include <sys/time.h>

namespace arrow {
namespace compute {
constexpr auto kSeed = 0x0ff1ce;

template <typename T>
static std::vector<T> load_data_chunked(const std::string& filename, std::vector<std::vector<T>>* datavec, int n_chunks) {
    std::vector<T> data;
    std::ifstream srcFile(filename, std::ios::in);
    if (!srcFile) {
        std::cout << "error opening source file." << std::endl;
        return data;
    }
    int counter = 0;
    while (srcFile.good()) {
        T next;
        srcFile >> next;
        if (!srcFile.good()) { break; }
        (*datavec)[counter%n_chunks].push_back(next);
        counter++;
    }
    srcFile.close();

    return data;
}
double getNow() {
  struct timeval tv;
  gettimeofday(&tv, 0);
  return tv.tv_sec + tv.tv_usec / 1000000.0;
}
    template <typename Type>
    std::shared_ptr<Array> merge(std::shared_ptr<ArraySpan> left, std::shared_ptr<ArraySpan> right, Type* result) {
        int left_size = left->length;
        int right_size = right->length;
        int i = 0, j = 0;
        Type left_val = 0;
        Type right_val = 0;
        int writeind = 0;
        while (i < left_size && j < right_size) {
          left_val = left->GetSingleValue<Type>(1, i);
          right_val = right->GetSingleValue<Type>(1, j);
            if (left_val <= right_val) {
                result[writeind] = (left_val);
                i++;
            } else {
                result[writeind] = (right_val);
                j++;
            }
            writeind++;
        }

        // Append any remaining elements from both vectors
        while (i < left_size) {
            left_val =  left->GetSingleValue<Type>(1, i);
            result[writeind] = (left_val);
            i++;
            writeind++;
        }
        while (j < right_size) {
            right_val =  right->GetSingleValue<Type>(1, j);
            result[writeind] = (right_val);
            j++;
            writeind++;
        }
        std::shared_ptr<Buffer> array_tmp = std::make_shared<Buffer>(reinterpret_cast<uint8_t*>(result),writeind*sizeof(Type));
        std::vector<std::shared_ptr<Buffer>> buffers = {nullptr, array_tmp};
        std::shared_ptr<ArrayData> data;
        if (sizeof(Type) == 4){
          data = ArrayData::Make(::arrow::int32(), writeind,
                                                      std::move(buffers), /*null_count=*/0, 0, CODEC::PLAIN);
        }
        else{
          data = ArrayData::Make(::arrow::int64(), writeind,
                                                      std::move(buffers), /*null_count=*/0, 0, CODEC::PLAIN);
        }
        
        ArraySpan data_span(*data);      
        return data_span.ToArray();
    }



void ChunkedArraySortIndicesInt64Benchmark(std::string file_path, int blocks, CODEC codec, int n_chunks) {
  std::vector<std::vector<int64_t>> datavec;
  datavec.reserve(n_chunks);
  for(int i = 0; i< n_chunks;i++){
    std::vector<int64_t> vec;
    vec.reserve(1000);
    datavec.push_back(vec);
  }
  std::vector<int64_t> data = load_data_chunked<int64_t>(file_path, &datavec, n_chunks);
  int64_t array_size = datavec[0].size();

  ArrayVector chunks;
  int blocks_ = blocks/n_chunks;
  for(int i=0;i<n_chunks;i++){
    Int64Builder builder;
    builder.SetCompress(codec);
    builder.AppendValues(datavec[i].data(), datavec[i].size(), nullptr, blocks_);
    std::shared_ptr<Array> out;
    builder.Finish(&out);
    chunks.push_back(std::move(out));
  }
  std::vector<std::shared_ptr<ArraySpan>> array_spans;
  for(auto item: chunks){
    array_spans.push_back(std::move(std::make_shared<ArraySpan>(*item->data())));
  }
  std::vector<int64_t*> datavec_new;
  datavec.reserve(n_chunks/2);
  for(int i = 0; i< n_chunks/2;i++){
    int64_t* ptr = new int64_t[datavec[i*2].size()+datavec[i*2+1].size()];
    datavec_new.push_back(ptr);
  }


  double start = getNow();            
  ArrayVector chunks_compress;
  if(codec != CODEC::PLAIN){
    // chunks.clear();
    for(int i=0;i<n_chunks/2;i++){  
      chunks_compress.push_back(std::move(merge<int64_t>(array_spans[i*2], array_spans[i*2+1], datavec_new[i])));
      datavec[i*2].clear();
      datavec[i*2+1].clear();
    }
    chunks.clear();
    auto result = SortIndices(*(std::make_shared<ChunkedArray>(chunks_compress)));
    const int64_t* result_sq = result.ValueOrDie()->data()->GetValues<int64_t>(1);
  }
  else{         
    auto result = SortIndices(*(std::make_shared<ChunkedArray>(chunks)));
    const int64_t* result_sq = result.ValueOrDie()->data()->GetValues<int64_t>(1);
  }       
  double end = getNow();
  std::cout<<file_path<<" "<<n_chunks<<" "<<codec<<" "<<(end - start)<<std::endl;
}





}  // namespace compute
}  // namespace arrow

int main(int argc, const char* argv[]){
  std::string file_path = argv[1];
  int blocks = atoi(argv[2]);
  int codec_num = atoi(argv[3]);
  int n_chunks = atoi(argv[4]);

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
  default:
    break;
  }
  
  arrow::compute::ChunkedArraySortIndicesInt64Benchmark("/root/arrow-private/cpp/Learn-to-Compress/data/"+file_path, blocks, codec, n_chunks);
}