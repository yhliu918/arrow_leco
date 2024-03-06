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

#include "benchmark/benchmark.h"

#include "arrow/compute/api_vector.h"
#include "arrow/compute/kernels/test_util.h"
#include "arrow/table.h"
#include "arrow/testing/gtest_util.h"
#include "arrow/testing/random.h"
#include "arrow/util/benchmark_util.h"
#include "arrow/util/logging.h"
#include <sys/time.h>

namespace arrow {
namespace compute {
constexpr auto kSeed = 0x0ff1ce;

static void ArraySortIndicesBenchmark(benchmark::State& state,
                                      const std::shared_ptr<Array>& values) {
  for (auto _ : state) {
    ABORT_NOT_OK(SortIndices(*values).status());
  }
  state.SetItemsProcessed(state.iterations() * values->length());
}

static void ChunkedArraySortIndicesBenchmark(
    benchmark::State& state, const std::shared_ptr<ChunkedArray>& values) {
  for (auto _ : state) {
    auto result = SortIndices(*values);
    const int64_t* result_sq = result.ValueOrDie()->data()->GetValues<int64_t>(1);
    std::cout<<result_sq[0]<<std::endl;
  }
  state.SetItemsProcessed(state.iterations() * values->length());
}

static void ArraySortIndicesInt64Benchmark(benchmark::State& state, int64_t min,
                                           int64_t max) {
  RegressionArgs args(state);

  const int64_t array_size = args.size / sizeof(int64_t);
  auto rand = random::RandomArrayGenerator(kSeed);
  auto values = rand.Int64(array_size, min, max, args.null_proportion);

  ArraySortIndicesBenchmark(state, values);
}

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
    std::shared_ptr<Array> merge(std::shared_ptr<ArraySpan> left, std::shared_ptr<ArraySpan> right) {
        int left_size = left->length;
        int right_size = right->length;
        Type* result = new Type[left_size+right_size];
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
            right_val =  right->GetSingleValue<Type>(1, i);
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



static void ChunkedArraySortIndicesInt64Benchmark(benchmark::State& state, int64_t min,
                                                  int64_t max) {
  RegressionArgs args(state);
  CODEC codec = CODEC::PLAIN;
  std::vector<std::vector<int64_t>> datavec;
  const int64_t n_chunks = 10;
  datavec.reserve(n_chunks);
  for(int i = 0; i< n_chunks;i++){
    std::vector<int64_t> vec;
    vec.reserve(1000);
    datavec.push_back(vec);
  }
  std::vector<int64_t> data = load_data_chunked<int64_t>("/root/arrow-private/cpp/Learn-to-Compress/data/wiki.txt", &datavec, n_chunks);
  int64_t array_size = datavec[0].size();

  ArrayVector chunks;
  for(int i=0;i<n_chunks;i++){
    Int64Builder builder;
    builder.SetCompress(codec);
    builder.AppendValues(datavec[i].data(), datavec[i].size(), nullptr, 100);
    std::shared_ptr<Array> out;
    builder.Finish(&out);
    chunks.push_back(std::move(out));
  }
  std::vector<std::shared_ptr<ArraySpan>> array_spans;
  for(auto item: chunks){
    // ArraySpan tmp(*item->data());
    array_spans.push_back(std::move(std::make_shared<ArraySpan>(*item->data())));
  }
  double start = getNow();            
  ArrayVector chunks_compress;
  if(codec != CODEC::PLAIN){
    for(int i=0;i<n_chunks/2;i++){  
      chunks_compress.push_back(std::move(merge<int64_t>(array_spans[i*2], array_spans[i*2+1])));
    }
    ChunkedArraySortIndicesBenchmark(state, std::make_shared<ChunkedArray>(chunks_compress));
  }
  else{         
    ChunkedArraySortIndicesBenchmark(state, std::make_shared<ChunkedArray>(chunks));
  }       
  double end = getNow();
  std::cout<<"sort"<<" "<<(end - start)<<std::endl;
}

static void ArraySortIndicesInt64Narrow(benchmark::State& state) {
  ArraySortIndicesInt64Benchmark(state, -100, 100);
}

static void ArraySortIndicesInt64Wide(benchmark::State& state) {
  const auto min = std::numeric_limits<int64_t>::min();
  const auto max = std::numeric_limits<int64_t>::max();
  ArraySortIndicesInt64Benchmark(state, min, max);
}

static void ArraySortIndicesBool(benchmark::State& state) {
  RegressionArgs args(state);

  const int64_t array_size = args.size * 8;
  auto rand = random::RandomArrayGenerator(kSeed);
  auto values = rand.Boolean(array_size, 0.5, args.null_proportion);

  ArraySortIndicesBenchmark(state, values);
}

static void ChunkedArraySortIndicesInt64Narrow(benchmark::State& state) {
  ChunkedArraySortIndicesInt64Benchmark(state, -100, 100);
}

static void ChunkedArraySortIndicesInt64Wide(benchmark::State& state) {
  const auto min = std::numeric_limits<int64_t>::min();
  const auto max = std::numeric_limits<int64_t>::max();
  ChunkedArraySortIndicesInt64Benchmark(state, min, max);
}

static void DatumSortIndicesBenchmark(benchmark::State& state, const Datum& datum,
                                      const SortOptions& options) {
  for (auto _ : state) {
    ABORT_NOT_OK(SortIndices(datum, options).status());
  }
}

// Extract benchmark args from benchmark::State
struct RecordBatchSortIndicesArgs {
  // the number of records
  const int64_t num_records;

  // proportion of nulls in generated arrays
  const double null_proportion;

  // the number of columns
  const int64_t num_columns;

  // Extract args
  explicit RecordBatchSortIndicesArgs(benchmark::State& state)
      : num_records(state.range(0)),
        null_proportion(ComputeNullProportion(state.range(1))),
        num_columns(state.range(2)),
        state_(state) {}

  ~RecordBatchSortIndicesArgs() {
    state_.counters["columns"] = static_cast<double>(num_columns);
    state_.counters["null_percent"] = null_proportion * 100;
    state_.SetItemsProcessed(state_.iterations() * num_records);
  }

 protected:
  double ComputeNullProportion(int64_t inverse_null_proportion) {
    if (inverse_null_proportion == 0) {
      return 0.0;
    } else {
      return std::min(1., 1. / static_cast<double>(inverse_null_proportion));
    }
  }

  benchmark::State& state_;
};

struct TableSortIndicesArgs : public RecordBatchSortIndicesArgs {
  // the number of chunks in each generated column
  const int64_t num_chunks;

  // Extract args
  explicit TableSortIndicesArgs(benchmark::State& state)
      : RecordBatchSortIndicesArgs(state), num_chunks(state.range(3)) {}

  ~TableSortIndicesArgs() { state_.counters["chunks"] = static_cast<double>(num_chunks); }
};

struct BatchOrTableBenchmarkData {
  std::shared_ptr<Schema> schema;
  std::vector<SortKey> sort_keys;
  ChunkedArrayVector columns;
};

BatchOrTableBenchmarkData MakeBatchOrTableBenchmarkDataInt64(
    const RecordBatchSortIndicesArgs& args, int64_t num_chunks, int64_t min_value,
    int64_t max_value) {
  auto rand = random::RandomArrayGenerator(kSeed);
  FieldVector fields;
  BatchOrTableBenchmarkData data;

  for (int64_t i = 0; i < args.num_columns; ++i) {
    auto name = std::to_string(i);
    fields.push_back(field(name, int64()));
    auto order = (i % 2) == 0 ? SortOrder::Ascending : SortOrder::Descending;
    data.sort_keys.emplace_back(name, order);
    ArrayVector chunks;
    if ((args.num_records % num_chunks) != 0) {
      Status::Invalid("The number of chunks (", num_chunks,
                      ") must be "
                      "a multiple of the number of records (",
                      args.num_records, ")")
          .Abort();
    }
    auto num_records_in_array = args.num_records / num_chunks;
    for (int64_t j = 0; j < num_chunks; ++j) {
      chunks.push_back(
          rand.Int64(num_records_in_array, min_value, max_value, args.null_proportion));
    }
    ASSIGN_OR_ABORT(auto chunked_array, ChunkedArray::Make(chunks, int64()));
    data.columns.push_back(chunked_array);
  }

  data.schema = schema(fields);
  return data;
}

static void RecordBatchSortIndicesInt64(benchmark::State& state, int64_t min,
                                        int64_t max) {
  RecordBatchSortIndicesArgs args(state);

  auto data = MakeBatchOrTableBenchmarkDataInt64(args, /*num_chunks=*/1, min, max);
  ArrayVector columns;
  for (const auto& chunked : data.columns) {
    ARROW_CHECK_EQ(chunked->num_chunks(), 1);
    columns.push_back(chunked->chunk(0));
  }

  auto batch = RecordBatch::Make(data.schema, args.num_records, columns);
  SortOptions options(data.sort_keys);
  DatumSortIndicesBenchmark(state, Datum(*batch), options);
}

static void TableSortIndicesInt64(benchmark::State& state, int64_t min, int64_t max) {
  TableSortIndicesArgs args(state);

  auto data = MakeBatchOrTableBenchmarkDataInt64(args, args.num_chunks, min, max);
  auto table = Table::Make(data.schema, data.columns, args.num_records);
  SortOptions options(data.sort_keys);
  DatumSortIndicesBenchmark(state, Datum(*table), options);
}

static void RecordBatchSortIndicesInt64Narrow(benchmark::State& state) {
  RecordBatchSortIndicesInt64(state, -100, 100);
}

static void RecordBatchSortIndicesInt64Wide(benchmark::State& state) {
  RecordBatchSortIndicesInt64(state, std::numeric_limits<int64_t>::min(),
                              std::numeric_limits<int64_t>::max());
}

static void TableSortIndicesInt64Narrow(benchmark::State& state) {
  TableSortIndicesInt64(state, -100, 100);
}

static void TableSortIndicesInt64Wide(benchmark::State& state) {
  TableSortIndicesInt64(state, std::numeric_limits<int64_t>::min(),
                        std::numeric_limits<int64_t>::max());
}

// BENCHMARK(ArraySortIndicesInt64Narrow)
//     ->Apply(RegressionSetArgs)
//     ->Args({1 << 20, 100})
//     ->Args({1 << 23, 100})
//     ->Unit(benchmark::TimeUnit::kNanosecond);

// BENCHMARK(ArraySortIndicesInt64Wide)
//     ->Apply(RegressionSetArgs)
//     ->Args({1 << 20, 100})
//     ->Args({1 << 23, 100})
//     ->Unit(benchmark::TimeUnit::kNanosecond);

// BENCHMARK(ArraySortIndicesBool)
//     ->Apply(RegressionSetArgs)
//     ->Args({1 << 20, 100})
//     ->Args({1 << 23, 100})
//     ->Unit(benchmark::TimeUnit::kNanosecond);
BENCHMARK(ChunkedArraySortIndicesInt64Narrow)
    ->Apply(RegressionSetArgs)
    ->Args({1 << 20, 100})
    ->Args({1 << 23, 100})
    ->Unit(benchmark::TimeUnit::kNanosecond);

// BENCHMARK(ChunkedArraySortIndicesInt64Wide)
//     ->Apply(RegressionSetArgs)
//     ->Args({1 << 20, 100})
//     ->Args({1 << 23, 100})
//     ->Unit(benchmark::TimeUnit::kNanosecond);

// BENCHMARK(RecordBatchSortIndicesInt64Narrow)
//     ->ArgsProduct({
//         {1 << 20},      // the number of records
//         {100, 4, 0},    // inverse null proportion
//         {16, 8, 2, 1},  // the number of columns
//     })
//     ->Unit(benchmark::TimeUnit::kNanosecond);

// BENCHMARK(RecordBatchSortIndicesInt64Wide)
//     ->ArgsProduct({
//         {1 << 20},      // the number of records
//         {100, 4, 0},    // inverse null proportion
//         {16, 8, 2, 1},  // the number of columns
//     })
//     ->Unit(benchmark::TimeUnit::kNanosecond);

// BENCHMARK(TableSortIndicesInt64Narrow)
//     ->ArgsProduct({
//         {1 << 20},      // the number of records
//         {100, 4, 0},    // inverse null proportion
//         {16, 8, 2, 1},  // the number of columns
//         {32, 4, 1},     // the number of chunks
//     })
//     ->Unit(benchmark::TimeUnit::kNanosecond);

// BENCHMARK(TableSortIndicesInt64Wide)
//     ->ArgsProduct({
//         {1 << 20},      // the number of records
//         {100, 4, 0},    // inverse null proportion
//         {16, 8, 2, 1},  // the number of columns
//         {32, 4, 1},     // the number of chunks
//     })
//     ->Unit(benchmark::TimeUnit::kNanosecond);

}  // namespace compute
}  // namespace arrow
