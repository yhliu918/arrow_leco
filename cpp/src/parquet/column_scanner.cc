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

#include "parquet/column_scanner.h"

#include <cstdint>
#include <memory>

#include "parquet/column_reader.h"

using arrow::MemoryPool;

namespace parquet {

std::shared_ptr<Scanner> Scanner::Make(std::shared_ptr<ColumnReader> col_reader,
                                       int64_t batch_size, MemoryPool* pool) {
  switch (col_reader->type()) {
    case Type::BOOLEAN:
      return std::make_shared<BoolScanner>(std::move(col_reader), batch_size, pool);
    case Type::INT32:
      return std::make_shared<Int32Scanner>(std::move(col_reader), batch_size, pool);
    case Type::INT64:
      return std::make_shared<Int64Scanner>(std::move(col_reader), batch_size, pool);
    case Type::INT96:
      return std::make_shared<Int96Scanner>(std::move(col_reader), batch_size, pool);
    case Type::FLOAT:
      return std::make_shared<FloatScanner>(std::move(col_reader), batch_size, pool);
    case Type::DOUBLE:
      return std::make_shared<DoubleScanner>(std::move(col_reader), batch_size, pool);
    case Type::BYTE_ARRAY:
      return std::make_shared<ByteArrayScanner>(std::move(col_reader), batch_size, pool);
    case Type::FIXED_LEN_BYTE_ARRAY:
      return std::make_shared<FixedLenByteArrayScanner>(std::move(col_reader), batch_size,
                                                        pool);
    default:
      ParquetException::NYI("type reader not implemented");
  }
  // Unreachable code, but suppress compiler warning
  return std::shared_ptr<Scanner>(nullptr);
}

int64_t ScanAllValues(int32_t batch_size, int16_t* def_levels, int16_t* rep_levels,
                      uint8_t* values, int64_t* values_buffered,
                      parquet::ColumnReader* reader) {
  switch (reader->type()) {
    case parquet::Type::BOOLEAN:
      return ScanAll<parquet::BoolReader>(batch_size, def_levels, rep_levels, values,
                                          values_buffered, reader);
    case parquet::Type::INT32:
      return ScanAll<parquet::Int32Reader>(batch_size, def_levels, rep_levels, values,
                                           values_buffered, reader);
    case parquet::Type::INT64:
      return ScanAll<parquet::Int64Reader>(batch_size, def_levels, rep_levels, values,
                                           values_buffered, reader);
    case parquet::Type::INT96:
      return ScanAll<parquet::Int96Reader>(batch_size, def_levels, rep_levels, values,
                                           values_buffered, reader);
    case parquet::Type::FLOAT:
      return ScanAll<parquet::FloatReader>(batch_size, def_levels, rep_levels, values,
                                           values_buffered, reader);
    case parquet::Type::DOUBLE:
      return ScanAll<parquet::DoubleReader>(batch_size, def_levels, rep_levels, values,
                                            values_buffered, reader);
    case parquet::Type::BYTE_ARRAY:
      return ScanAll<parquet::ByteArrayReader>(batch_size, def_levels, rep_levels, values,
                                               values_buffered, reader);
    case parquet::Type::FIXED_LEN_BYTE_ARRAY:
      return ScanAll<parquet::FixedLenByteArrayReader>(batch_size, def_levels, rep_levels,
                                                       values, values_buffered, reader);
    default:
      parquet::ParquetException::NYI("type reader not implemented");
  }
  // Unreachable code, but suppress compiler warning
  return 0;
}

int64_t ScanAllValuesBitpos(int32_t batch_size, int16_t* def_levels, int16_t* rep_levels,
                            uint8_t* values, int64_t* values_buffered,
                            parquet::ColumnReader* reader, int64_t* values_true_read,
                            std::vector<uint32_t>& bitpos, int64_t row_index, int64_t bitpos_index) {
  switch (reader->type()) {
    case parquet::Type::INT32:
      return ScanAllBitpos<parquet::Int32Reader>(batch_size, def_levels, rep_levels,
                                                 values, values_buffered, reader,
                                                 values_true_read, bitpos, row_index, bitpos_index);
    case parquet::Type::INT64:
      return ScanAllBitpos<parquet::Int64Reader>(batch_size, def_levels, rep_levels,
                                                 values, values_buffered, reader,
                                                 values_true_read, bitpos, row_index, bitpos_index);
    default:
      parquet::ParquetException::NYI("type reader not implemented");
  }
  return 0;
}

int64_t FilterScanAllValues(int32_t batch_size, int16_t* def_levels, int16_t* rep_levels,
                      uint8_t* values, int64_t* values_buffered,
                      parquet::ColumnReader* reader, int64_t filter_val, std::vector<uint32_t>& bitpos, bool is_gt, int64_t* filter_count){
  switch (reader->type()) {
    case parquet::Type::INT32:
      return FilterScanAll<parquet::Int32Reader>(batch_size, def_levels, rep_levels,
                                                 values, values_buffered, reader,
                                                 filter_val, bitpos, is_gt, filter_count);
    case parquet::Type::INT64:
      return FilterScanAll<parquet::Int64Reader>(batch_size, def_levels, rep_levels,
                                                 values, values_buffered, reader,
                                                 filter_val, bitpos, is_gt, filter_count);
    default:
      parquet::ParquetException::NYI("type reader not implemented");
  }
  return 0;
}

}  // namespace parquet
