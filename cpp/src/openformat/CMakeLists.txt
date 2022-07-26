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


add_library(gprof SHARED IMPORTED)
set_target_properties(gprof PROPERTIES IMPORTED_LOCATION /usr/local/lib/libprofiler.so)

set(LIBS ${PARQUET_EXAMPLE_LINK_LIBS} arrow_shared ${DATASET_EXAMPLES_LINK_LIBS} gprof)

target_link_libraries(pq_writer ${LIBS})
target_link_libraries(pq_reader ${LIBS})
target_link_libraries(orc_reader ${LIBS})
target_link_libraries(buff_test ${LIBS})
target_link_libraries(raw_reader ${LIBS})
target_link_libraries(raw_reader_v2 ${LIBS})
target_link_libraries(rle_test ${LIBS})


add_dependencies(parquet pq_writer pq_reader orc_reader)
