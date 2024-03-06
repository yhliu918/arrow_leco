# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

# ----------------------------------------------------------------------
# Scalar kernels

add_arrow_compute_test(scalar_test
                       SOURCES
                       scalar_arithmetic_test.cc
                       scalar_boolean_test.cc
                       scalar_cast_test.cc
                       scalar_compare_test.cc
                       scalar_if_else_test.cc
                       scalar_nested_test.cc
                       scalar_random_test.cc
                       scalar_set_lookup_test.cc
                       scalar_string_test.cc
                       scalar_temporal_test.cc
                       scalar_validity_test.cc
                       test_util.cc)

add_arrow_benchmark(scalar_arithmetic_benchmark PREFIX "arrow-compute")
add_arrow_benchmark(scalar_boolean_benchmark PREFIX "arrow-compute")
add_arrow_benchmark(scalar_cast_benchmark PREFIX "arrow-compute")
add_arrow_benchmark(scalar_compare_benchmark PREFIX "arrow-compute")
add_arrow_benchmark(scalar_if_else_benchmark PREFIX "arrow-compute")
add_arrow_benchmark(scalar_random_benchmark PREFIX "arrow-compute")
add_arrow_benchmark(scalar_set_lookup_benchmark PREFIX "arrow-compute")
add_arrow_benchmark(scalar_string_benchmark PREFIX "arrow-compute")
add_arrow_benchmark(scalar_temporal_benchmark PREFIX "arrow-compute")

# ----------------------------------------------------------------------
# Vector kernels

add_arrow_compute_test(vector_test
                       SOURCES
                       vector_cumulative_ops_test.cc
                       vector_hash_test.cc
                       vector_nested_test.cc
                       vector_replace_test.cc
                       vector_selection_test.cc
                       vector_sort_test.cc
                       select_k_test.cc
                       test_util.cc)

add_arrow_benchmark(vector_hash_benchmark PREFIX "arrow-compute")
add_arrow_benchmark(vector_sort_benchmark PREFIX "arrow-compute")
add_arrow_benchmark(vector_partition_benchmark PREFIX "arrow-compute")
add_arrow_benchmark(vector_topk_benchmark PREFIX "arrow-compute")
add_arrow_benchmark(vector_replace_benchmark PREFIX "arrow-compute")
add_arrow_benchmark(vector_selection_benchmark PREFIX "arrow-compute")
add_arrow_benchmark(vector_selection_codec_benchmark PREFIX "arrow-compute")

# ----------------------------------------------------------------------
# Aggregate kernels

# Aggregates

add_arrow_compute_test(aggregate_test
                       SOURCES
                       aggregate_test.cc
                       hash_aggregate_test.cc
                       test_util.cc)
add_arrow_benchmark(aggregate_benchmark PREFIX "arrow-compute")
add_arrow_benchmark(aggregate_codec_benchmark PREFIX "arrow-compute")

add_executable(aggregate_codec aggregate_codec_benchmark.cc)
target_link_libraries(aggregate_codec arrow_shared GTest::gtest)

add_executable(sort_codec vector_sort_codec.cc)
target_link_libraries(sort_codec arrow_shared GTest::gtest)

add_executable(take_codec vector_selection_codec_benchmark.cc)
target_link_libraries(take_codec arrow_shared GTest::gtest ${ARROW_TEST_LINK_LIBS})

add_executable(filter_codec vector_filter_codec_benchmark.cc)
target_link_libraries(filter_codec arrow_shared GTest::gtest ${ARROW_TEST_LINK_LIBS})

add_executable(groupby_codec aggregate_groupby_codec.cc)
target_link_libraries(groupby_codec arrow_shared GTest::gtest ${ARROW_TEST_LINK_LIBS})
# ----------------------------------------------------------------------
# Utilities

add_arrow_compute_test(kernel_utility_test SOURCES codegen_internal_test.cc)
