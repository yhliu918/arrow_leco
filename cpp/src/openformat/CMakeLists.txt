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



add_executable(pq_writer pq_writer.cc)
add_executable(pq_reader pq_reader.cc)
add_executable(orc_reader orc_reader.cc)
add_executable(buff_test buffer_builder_test.cc)
add_executable(raw_reader raw_reader.cc)
add_executable(raw_reader_v2 raw_reader_v2.cc)
add_executable(rle_test rle_test.cc)
add_executable(for for.cc)
add_executable(for_64 for_64.cc)
add_executable(for_64_old for_64_old.cc)
add_executable(take_bench take_bench.cc)
add_executable(filter_bench filter_bench.cc)
add_executable(aggsum_bench aggsum_bench.cc)
add_executable(for_64_zstd for_64_zstd.cc)
add_executable(filter_int filter_int.cc)
add_executable(filter_agg_int filter_agg_int.cc)
add_executable(filter_groupbysum filter_groupbysum.cc)
add_executable(filter_groupbysum_chunked filter_groupbysum_chunked.cc)
add_executable(filter_bitscan_groupby filter_bitscan_groupby.cc)
add_executable(test_binary test_binary.cc)

# target_include_directories(parquet-low-level-example PRIVATE low_level_api/)
# target_include_directories(parquet-low-level-example2 PRIVATE low_level_api/)


# Prefer shared linkage but use static if shared build is deactivated
if(ARROW_BUILD_SHARED)
    set(DATASET_EXAMPLES_LINK_LIBS arrow_dataset_shared)
    set(PARQUET_EXAMPLE_LINK_LIBS parquet_shared)
else()
    set(DATASET_EXAMPLES_LINK_LIBS arrow_dataset_static)
    set(PARQUET_EXAMPLE_LINK_LIBS parquet_static)
endif()


# add_library(gprof SHARED IMPORTED)
# set_target_properties(gprof PROPERTIES IMPORTED_LOCATION /usr/local/lib/libprofiler.so)

set(LIBS ${PARQUET_EXAMPLE_LINK_LIBS} arrow_shared ${DATASET_EXAMPLES_LINK_LIBS})

target_link_libraries(pq_writer ${LIBS})
target_link_libraries(pq_reader ${LIBS})
target_link_libraries(orc_reader ${LIBS})
target_link_libraries(buff_test ${LIBS})
target_link_libraries(raw_reader ${LIBS})
target_link_libraries(raw_reader_v2 ${LIBS})
target_link_libraries(rle_test ${LIBS})
target_link_libraries(for ${LIBS})
target_link_libraries(for_64 ${LIBS})
target_link_libraries(for_64_old ${LIBS})
target_link_libraries(take_bench ${LIBS})
target_link_libraries(filter_bench ${LIBS})
target_link_libraries(aggsum_bench ${LIBS})
target_link_libraries(for_64_zstd ${LIBS})
target_link_libraries(filter_int ${LIBS})
target_link_libraries(filter_agg_int ${LIBS})
target_link_libraries(filter_groupbysum ${LIBS})
target_link_libraries(filter_bitscan_groupby ${LIBS})
target_link_libraries(filter_groupbysum_chunked ${LIBS})


add_dependencies(parquet pq_writer pq_reader orc_reader)
