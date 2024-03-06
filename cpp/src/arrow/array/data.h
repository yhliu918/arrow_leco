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

#pragma once

#include <atomic>  // IWYU pragma: export
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "arrow/buffer.h"
#include "arrow/result.h"
#include "arrow/type.h"
#include "arrow/type_traits.h"
#include "arrow/util/bit_util.h"
#include "arrow/util/macros.h"
#include "arrow/util/visibility.h"
#include "arrow/leco/FOR_integer_template.h"
#include "arrow/leco/piecewise_fix_integer_template.h"
#include "arrow/leco/delta_integer_template.h"

namespace arrow {

class Array;

// When slicing, we do not know the null count of the sliced range without
// doing some computation. To avoid doing this eagerly, we set the null count
// to -1 (any negative number will do). When Array::null_count is called the
// first time, the null count will be computed. See ARROW-33
constexpr int64_t kUnknownNullCount = -1;

// ----------------------------------------------------------------------
// Generic array data container

/// \class ArrayData
/// \brief Mutable container for generic Arrow array data
///
/// This data structure is a self-contained representation of the memory and
/// metadata inside an Arrow array data structure (called vectors in Java). The
/// classes arrow::Array and its subclasses provide strongly-typed accessors
/// with support for the visitor pattern and other affordances.
///
/// This class is designed for easy internal data manipulation, analytical data
/// processing, and data transport to and from IPC messages. For example, we
/// could cast from int64 to float64 like so:
///
/// Int64Array arr = GetMyData();
/// auto new_data = arr.data()->Copy();
/// new_data->type = arrow::float64();
/// DoubleArray double_arr(new_data);
///
/// This object is also useful in an analytics setting where memory may be
/// reused. For example, if we had a group of operations all returning doubles,
/// say:
///
/// Log(Sqrt(Expr(arr)))
///
/// Then the low-level implementations of each of these functions could have
/// the signatures
///
/// void Log(const ArrayData& values, ArrayData* out);
///
/// As another example a function may consume one or more memory buffers in an
/// input array and replace them with newly-allocated data, changing the output
/// data type as well.
struct ARROW_EXPORT ArrayData {
  ArrayData() = default;

  ArrayData(std::shared_ptr<DataType> type, int64_t length,
            int64_t null_count = kUnknownNullCount, int64_t offset = 0)
      : type(std::move(type)), length(length), null_count(null_count), offset(offset) {}


  ArrayData(std::shared_ptr<DataType> type, int64_t length,
            std::vector<std::shared_ptr<Buffer>> buffers,
            int64_t null_count = kUnknownNullCount, int64_t offset = 0, CODEC codec=CODEC::PLAIN)
      : ArrayData(std::move(type), length, null_count, offset) {
    this->buffers = std::move(buffers);
    this->compression_type = codec;
  }

  ArrayData(std::shared_ptr<DataType> type, int64_t length,
            std::vector<std::shared_ptr<Buffer>> buffers,
            std::vector<std::shared_ptr<ArrayData>> child_data,
            int64_t null_count = kUnknownNullCount, int64_t offset = 0)
      : ArrayData(std::move(type), length, null_count, offset) {
    this->buffers = std::move(buffers);
    this->child_data = std::move(child_data);
  }

  static std::shared_ptr<ArrayData> Make(std::shared_ptr<DataType> type, int64_t length,
                                         std::vector<std::shared_ptr<Buffer>> buffers,
                                         int64_t null_count = kUnknownNullCount,
                                         int64_t offset = 0, CODEC codec=CODEC::PLAIN);

  static std::shared_ptr<ArrayData> Make(
      std::shared_ptr<DataType> type, int64_t length,
      std::vector<std::shared_ptr<Buffer>> buffers,
      std::vector<std::shared_ptr<ArrayData>> child_data,
      int64_t null_count = kUnknownNullCount, int64_t offset = 0);

  static std::shared_ptr<ArrayData> Make(
      std::shared_ptr<DataType> type, int64_t length,
      std::vector<std::shared_ptr<Buffer>> buffers,
      std::vector<std::shared_ptr<ArrayData>> child_data,
      std::shared_ptr<ArrayData> dictionary, int64_t null_count = kUnknownNullCount,
      int64_t offset = 0);

  static std::shared_ptr<ArrayData> Make(std::shared_ptr<DataType> type, int64_t length,
                                         int64_t null_count = kUnknownNullCount,
                                         int64_t offset = 0);

  // Move constructor
  ArrayData(ArrayData&& other) noexcept
      : type(std::move(other.type)),
        length(other.length),
        offset(other.offset),
        buffers(std::move(other.buffers)),
        child_data(std::move(other.child_data)),
        dictionary(std::move(other.dictionary)),
        compression_type(other.compression_type) {
    SetNullCount(other.null_count);
  }

  // Copy constructor
  ArrayData(const ArrayData& other) noexcept
      : type(other.type),
        length(other.length),
        offset(other.offset),
        buffers(other.buffers),
        child_data(other.child_data),
        dictionary(other.dictionary),
        compression_type(other.compression_type) {
    SetNullCount(other.null_count);
  }

  // Move assignment
  ArrayData& operator=(ArrayData&& other) {
    type = std::move(other.type);
    length = other.length;
    SetNullCount(other.null_count);
    offset = other.offset;
    buffers = std::move(other.buffers);
    child_data = std::move(other.child_data);
    dictionary = std::move(other.dictionary);
    compression_type = other.compression_type;
    return *this;
  }

  // Copy assignment
  ArrayData& operator=(const ArrayData& other) {
    type = other.type;
    length = other.length;
    SetNullCount(other.null_count);
    offset = other.offset;
    buffers = other.buffers;
    child_data = other.child_data;
    dictionary = other.dictionary;
    compression_type = other.compression_type;
    return *this;
  }

  std::shared_ptr<ArrayData> Copy() const { return std::make_shared<ArrayData>(*this); }

  // Access a buffer's data as a typed C pointer
  template <typename T>
  inline const T* GetValues(int i, int64_t absolute_offset) const {
    if (buffers[i]) {
      return reinterpret_cast<const T*>(buffers[i]->data()) + absolute_offset;
    } else {
      return NULLPTR;
    }
  }

  template <typename T>
  inline const T* GetValues(int i) const {
    return GetValues<T>(i, offset);
  }


  // Like GetValues, but returns NULLPTR instead of aborting if the underlying
  // buffer is not a CPU buffer.
  template <typename T>
  inline const T* GetValuesSafe(int i, int64_t absolute_offset) const {
    if (buffers[i] && buffers[i]->is_cpu()) {
      return reinterpret_cast<const T*>(buffers[i]->data()) + absolute_offset;
    } else {
      return NULLPTR;
    }
  }

  template <typename T>
  inline const T* GetValuesSafe(int i) const {
    return GetValuesSafe<T>(i, offset);
  }

  // Access a buffer's data as a typed C pointer
  template <typename T>
  inline T* GetMutableValues(int i, int64_t absolute_offset) {
    if (buffers[i]) {
      return reinterpret_cast<T*>(buffers[i]->mutable_data()) + absolute_offset;
    } else {
      return NULLPTR;
    }
  }

  template <typename T>
  inline T* GetMutableValues(int i) {
    return GetMutableValues<T>(i, offset);
  }

  /// \brief Construct a zero-copy slice of the data with the given offset and length
  std::shared_ptr<ArrayData> Slice(int64_t offset, int64_t length) const;

  /// \brief Input-checking variant of Slice
  ///
  /// An Invalid Status is returned if the requested slice falls out of bounds.
  /// Note that unlike Slice, `length` isn't clamped to the available buffer size.
  Result<std::shared_ptr<ArrayData>> SliceSafe(int64_t offset, int64_t length) const;

  void SetNullCount(int64_t v) { null_count.store(v); }

  /// \brief Return null count, or compute and set it if it's not known
  int64_t GetNullCount() const;

  bool MayHaveNulls() const {
    // If an ArrayData is slightly malformed it may have kUnknownNullCount set
    // but no buffer
    return null_count.load() != 0 && buffers[0] != NULLPTR;
  }

  void SetCompressionType(CODEC codec);

  CODEC compression_type = CODEC::PLAIN;
  std::shared_ptr<DataType> type;
  int64_t length = 0;
  mutable std::atomic<int64_t> null_count{0};
  // The logical start point into the physical buffers (in values, not bytes).
  // Note that, for child data, this must be *added* to the child data's own offset.
  int64_t offset = 0;
  std::vector<std::shared_ptr<Buffer>> buffers;
  std::vector<std::shared_ptr<ArrayData>> child_data;

  // The dictionary for this Array, if any. Only used for dictionary type
  std::shared_ptr<ArrayData> dictionary;
};

/// \brief A non-owning Buffer reference
struct ARROW_EXPORT BufferSpan {
  // It is the user of this class's responsibility to ensure that
  // buffers that were const originally are not written to
  // accidentally.
  uint8_t* data = NULLPTR;
  int64_t size = 0;
  // Pointer back to buffer that owns this memory
  const std::shared_ptr<Buffer>* owner = NULLPTR;
};

/// \brief EXPERIMENTAL: A non-owning ArrayData reference that is cheaply
/// copyable and does not contain any shared_ptr objects. Do not use in public
/// APIs aside from compute kernels for now
struct ARROW_EXPORT ArraySpan {
  const DataType* type = NULLPTR;
  int64_t length = 0;
  mutable int64_t null_count = kUnknownNullCount;
  int64_t offset = 0;
  BufferSpan buffers[3];
  CODEC compression_type = CODEC::PLAIN;
  mutable Codecset::FOR_int<int64_t> codec_for;
  mutable Codecset::Leco_int<int64_t> codec_leco;
  mutable Codecset::Delta_int<int64_t> codec_delta;
  mutable int* start_pos = nullptr;
  mutable int blocks = 0;
  mutable int block_size = 0;
  mutable bool codec_inited = false;

  // 16 bytes of scratch space to enable this ArraySpan to be a view onto
  // scalar values including binary scalars (where we need to create a buffer
  // that looks like two 32-bit or 64-bit offsets)
  uint64_t scratch_space[2];

  ArraySpan() = default;

  explicit ArraySpan(const DataType* type, int64_t length) : type(type), length(length) {}

  ArraySpan(const ArrayData& data) {  // NOLINT implicit conversion
    SetMembers(data);
  }
  explicit ArraySpan(const Scalar& data) { FillFromScalar(data); }

  /// If dictionary-encoded, put dictionary in the first entry
  std::vector<ArraySpan> child_data;

  /// \brief Populate ArraySpan to look like an array of length 1 pointing at
  /// the data members of a Scalar value
  void FillFromScalar(const Scalar& value);

  void SetMembers(const ArrayData& data);

  void SetCompressionType(CODEC codec){compression_type = codec;}

  void SetBuffer(int index, const std::shared_ptr<Buffer>& buffer) {
    this->buffers[index].data = const_cast<uint8_t*>(buffer->data());
    this->buffers[index].size = buffer->size();
    this->buffers[index].owner = &buffer;
  }

  const ArraySpan& dictionary() const { return child_data[0]; }

  /// \brief Return the number of buffers (out of 3) that are used to
  /// constitute this array
  int num_buffers() const;

  // Access a buffer's data as a typed C pointer
  template <typename T>
  inline T* GetValues(int i, int64_t absolute_offset) {
    return reinterpret_cast<T*>(buffers[i].data) + absolute_offset;
  }

  template <typename T>
  inline T* GetValues(int i) {
    return GetValues<T>(i, this->offset);
  }

  template <typename T>
  inline int64_t GetCompressedValuesRange(int i, int pos, int len) const {
    if (compression_type == CODEC::DELTA){
      int64_t sum =0;
      T* out = new T[len];
      Codecset::Delta_int<T> codec;
      uint8_t* data_buffer = buffers[i].data;
      int blocks = 0;
      int block_size = 0;
      memcpy(&blocks, data_buffer, sizeof(int));
      memcpy(&block_size, data_buffer+sizeof(int), sizeof(int));
      int* start_pos = reinterpret_cast<int*>(data_buffer+sizeof(int)*2);
      codec.init(blocks, block_size);

      int start_idx = pos;
      int end_idx = pos+len-1;
      int start_block = start_idx/block_size;
      int end_block = end_idx / block_size;
      if(start_block==end_block){
        codec.decodeRange(data_buffer+ start_pos[start_block], out, start_idx%block_size, len);
      }
      else{
        codec.decodeRange(data_buffer+ start_pos[start_block], out, start_idx%block_size, (block_size - start_idx%block_size));
        out += (block_size - start_idx%block_size);
        for(int i = start_block+1; i< end_block;i++){
          codec.decodeRange(data_buffer+ start_pos[i], out, 0, block_size);
          out+= block_size;
        }
        codec.decodeRange(data_buffer+ start_pos[end_block], out, 0, end_idx%block_size);
      }
      for (int64_t i = 0; i < len; ++i) {
          sum += out[i];
      }
      free(out);
      return sum;
    }
  }
  template <typename T>
  inline void DecodeBlock(int blockid, T* out) const {
      Codecset::Delta_int<T> codec;
      uint8_t* data_buffer = buffers[1].data;
      int blocks = 0;
      int block_size = 0;
      memcpy(&blocks, data_buffer, sizeof(int));
      memcpy(&block_size, data_buffer+sizeof(int), sizeof(int));
      int start_pos = reinterpret_cast<int*>(data_buffer+sizeof(int)*2)[blockid];
      codec.init(blocks, block_size);
      codec.decodeArray8(data_buffer+start_pos, block_size, out, 0);

  }

  template <typename T>
  inline void GetCompressedValues(int i, T* out) const {
    if (compression_type == CODEC::FOR){
      Codecset::FOR_int<T> codec;
      uint8_t* data_buffer = buffers[i].data;
      int blocks = 0;
      int block_size = 0;
      memcpy(&blocks, data_buffer, sizeof(int));
      memcpy(&block_size, data_buffer+sizeof(int), sizeof(int));
      int* start_pos = reinterpret_cast<int*>(data_buffer+sizeof(int)*2);
      codec.init(blocks, block_size);
      int block_length = block_size;
      for(int i = 0; i< blocks;i++){
        if (i == blocks - 1)
        {
            block_length = length - (blocks - 1) * block_size;
        }
        int seg_start = start_pos[i];
        codec.decodeArray8(data_buffer+seg_start, block_length, out + i * block_size, i);
      }
    }
    else if (compression_type == CODEC::LECO){
      Codecset::Leco_int<T> codec;
      uint8_t* data_buffer = buffers[i].data;
      int blocks = 0;
      int block_size = 0;
      memcpy(&blocks, data_buffer, sizeof(int));
      memcpy(&block_size, data_buffer+sizeof(int), sizeof(int));
      int* start_pos = reinterpret_cast<int*>(data_buffer+sizeof(int)*2);
      codec.init(blocks, block_size);
      int block_length = block_size;
      for(int i = 0; i< blocks;i++){
        if (i == blocks - 1)
        {
            block_length = length - (blocks - 1) * block_size;
        }
        int seg_start = start_pos[i];
        codec.decodeArray8(data_buffer+seg_start, block_length, out + i * block_size, i);
      }
    }
    else if (compression_type == CODEC::DELTA){
      Codecset::Delta_int<T> codec;
      uint8_t* data_buffer = buffers[i].data;
      int blocks = 0;
      int block_size = 0;
      memcpy(&blocks, data_buffer, sizeof(int));
      memcpy(&block_size, data_buffer+sizeof(int), sizeof(int));
      int* start_pos = reinterpret_cast<int*>(data_buffer+sizeof(int)*2);
      codec.init(blocks, block_size);
      int block_length = block_size;
      for(int i = 0; i< blocks;i++){
        if (i == blocks - 1)
        {
            block_length = length - (blocks - 1) * block_size;
        }
        int seg_start = start_pos[i];
        codec.decodeArray8(data_buffer+seg_start, block_length, out + i * block_size, i);
      }
    }
  }

  template <typename T>
  inline T GetSum(int i) const {
    if (compression_type == CODEC::FOR){
      T returnvalue = 0;
      Codecset::FOR_int<T> codec;
      uint8_t* data_buffer = buffers[i].data;
      int blocks = 0;
      int block_size = 0;
      memcpy(&blocks, data_buffer, sizeof(int));
      memcpy(&block_size, data_buffer+sizeof(int), sizeof(int));
      int* start_pos = reinterpret_cast<int*>(data_buffer+sizeof(int)*2);
      codec.init(blocks, block_size);
      int block_length = block_size;
      for(int i = 0; i< blocks;i++){
        if (i == blocks - 1)
        {
            block_length = length - (blocks - 1) * block_size;
        }
        int seg_start = start_pos[i];
        returnvalue+= codec.summation(data_buffer+seg_start, block_length, i);
      }
      return returnvalue;
    }
    else if (compression_type == CODEC::LECO){
      T returnvalue = 0;
      Codecset::Leco_int<T> codec;
      uint8_t* data_buffer = buffers[i].data;
      int blocks = 0;
      int block_size = 0;
      memcpy(&blocks, data_buffer, sizeof(int));
      memcpy(&block_size, data_buffer+sizeof(int), sizeof(int));
      int* start_pos = reinterpret_cast<int*>(data_buffer+sizeof(int)*2);
      codec.init(blocks, block_size);
      int block_length = block_size;
      for(int i = 0; i< blocks;i++){
        if (i == blocks - 1)
        {
            block_length = length - (blocks - 1) * block_size;
        }
        int seg_start = start_pos[i];
        returnvalue+= codec.summation(data_buffer+seg_start, block_length, i);
      }
      return returnvalue;
    }
    
  }

  void init_codec() const {
    if(compression_type!=CODEC::PLAIN){
    uint8_t* data_buffer = buffers[1].data;
    memcpy(&blocks, data_buffer, sizeof(int));
    memcpy(&block_size, data_buffer+sizeof(int), sizeof(int));
    if(compression_type==CODEC::FOR){
      codec_for.init(blocks, block_size);
    }
    else if(compression_type==CODEC::LECO){
      codec_leco.init(blocks, block_size);
    }
    else if(compression_type==CODEC::DELTA){
      codec_delta.init(blocks, block_size);
    }

    start_pos = reinterpret_cast<int*>(data_buffer+sizeof(int)*2);
    codec_inited = true;
    }
  }
  template <typename T>
  inline T GetSumRange(int i, int start_idx, int end_idx) const {
    if (compression_type == CODEC::FOR){
      T returnvalue = 0;
      uint8_t* data_buffer = buffers[i].data;
      int block_length = block_size;

      int start_block = start_idx/block_size;
      int end_block = end_idx / block_size;
      if(start_block==end_block){
        returnvalue = codec_for.summation_range(data_buffer+ start_pos[start_block], start_idx%block_size, end_idx%block_size);
      }
      else{
        returnvalue += codec_for.summation_range(data_buffer+ start_pos[start_block], start_idx%block_size, block_size - 1);
        returnvalue += codec_for.summation_range(data_buffer+ start_pos[end_block], 0, end_idx%block_size);
        for(int i = start_block+1; i< end_block;i++){
          int seg_start = start_pos[i];
          returnvalue+= codec_for.summation(data_buffer+seg_start, block_length, i);
        }
      }
      return returnvalue;
    }
    else if (compression_type == CODEC::LECO){
      T returnvalue = 0;
      uint8_t* data_buffer = buffers[i].data;
      int block_length = block_size;

      int start_block = start_idx/block_size;
      int end_block = end_idx / block_size;
      if(start_block==end_block){
        returnvalue = codec_leco.summation_range(data_buffer+ start_pos[start_block], start_idx%block_size, end_idx%block_size);
      }
      else{
        returnvalue += codec_leco.summation_range(data_buffer+ start_pos[start_block], start_idx%block_size, block_size - 1);
        returnvalue += codec_leco.summation_range(data_buffer+ start_pos[end_block], 0, end_idx%block_size);
        for(int i = start_block+1; i< end_block;i++){
          int seg_start = start_pos[i];
          returnvalue+= codec_leco.summation(data_buffer+seg_start, block_length, i);
        }
      }
      return returnvalue;
    }
    else if (compression_type == CODEC::DELTA){
      T returnvalue = 0;
      uint8_t* data_buffer = buffers[i].data;
      int block_length = block_size;

      int start_block = start_idx/block_size;
      int end_block = end_idx / block_size;
      if(start_block==end_block){
        returnvalue = codec_delta.summation_range(data_buffer+ start_pos[start_block], start_idx%block_size, end_idx%block_size);
      }
      else{
        returnvalue += codec_delta.summation_range(data_buffer+ start_pos[start_block], start_idx%block_size, block_size - 1);
        returnvalue += codec_delta.summation_range(data_buffer+ start_pos[end_block], 0, end_idx%block_size);
        for(int i = start_block+1; i< end_block;i++){
          int seg_start = start_pos[i];
          returnvalue+= codec_delta.summation(data_buffer+seg_start, block_length, i);
        }
      }
      return returnvalue;
    }
    
  }

  // Access a buffer's data as a typed C pointer
  template <typename T>
  inline const T* GetValues(int i, int64_t absolute_offset) const {
    return reinterpret_cast<const T*>(buffers[i].data) + absolute_offset;
  }

  template <typename T>
  inline const T* GetValues(int i) const {
    return GetValues<T>(i, this->offset);
  }

  template <typename T>
  inline const T GetSingleValue(int i, int idx) const {
    if (compression_type==CODEC::PLAIN){
        return GetValues<T>(i, this->offset)[idx];
    }
    else if (compression_type == CODEC::FOR){
      if(!codec_inited){
        init_codec();
      }
      uint8_t* data_buffer = buffers[i].data;
      int block_idx = idx/block_size;
      T tmpvalue = codec_for.randomdecodeArray8(data_buffer+start_pos[block_idx], idx % block_size, NULL, length);
      return tmpvalue;

    }
    else if (compression_type == CODEC::LECO){
      if(!codec_inited){
        init_codec();
      }
      uint8_t* data_buffer = buffers[i].data;
      int block_idx = idx/block_size;
      T tmpvalue = codec_leco.randomdecodeArray8(data_buffer+start_pos[block_idx], idx % block_size, NULL, length);
      return tmpvalue;

    }
    else if (compression_type == CODEC::DELTA){
      if(!codec_inited){
        init_codec();
      }
      uint8_t* data_buffer = buffers[i].data;
      int block_idx = idx/block_size;
      T tmpvalue = codec_delta.randomdecodeArray8(data_buffer+start_pos[block_idx], idx % block_size, NULL, length);
      return tmpvalue;

    }
  }

  bool IsNull(int64_t i) const {
    return ((this->buffers[0].data != NULLPTR)
                ? !bit_util::GetBit(this->buffers[0].data, i + this->offset)
                : this->null_count == this->length);
  }

  bool IsValid(int64_t i) const {
    return ((this->buffers[0].data != NULLPTR)
                ? bit_util::GetBit(this->buffers[0].data, i + this->offset)
                : this->null_count != this->length);
  }

  std::shared_ptr<ArrayData> ToArrayData() const;

  std::shared_ptr<Array> ToArray() const;

  std::shared_ptr<Buffer> GetBuffer(int index) const {
    const BufferSpan& buf = this->buffers[index];
    if (buf.owner) {
      return *buf.owner;
    } else if (buf.data != NULLPTR) {
      // Buffer points to some memory without an owning buffer
      return std::make_shared<Buffer>(buf.data, buf.size);
    } else {
      return NULLPTR;
    }
  }

  void SetSlice(int64_t offset, int64_t length) {
    this->offset = offset;
    this->length = length;
    if (this->type->id() != Type::NA) {
      this->null_count = kUnknownNullCount;
    } else {
      this->null_count = this->length;
    }
  }

  /// \brief Return null count, or compute and set it if it's not known
  int64_t GetNullCount() const;

  bool MayHaveNulls() const {
    // If an ArrayData is slightly malformed it may have kUnknownNullCount set
    // but no buffer
    return null_count != 0 && buffers[0].data != NULLPTR;
  }
};

namespace internal {

void FillZeroLengthArray(const DataType* type, ArraySpan* span);

/// Construct a zero-copy view of this ArrayData with the given type.
///
/// This method checks if the types are layout-compatible.
/// Nested types are traversed in depth-first order. Data buffers must have
/// the same item sizes, even though the logical types may be different.
/// An error is returned if the types are not layout-compatible.
ARROW_EXPORT
Result<std::shared_ptr<ArrayData>> GetArrayView(const std::shared_ptr<ArrayData>& data,
                                                const std::shared_ptr<DataType>& type);

}  // namespace internal
}  // namespace arrow
