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

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "arrow/array/array_base.h"
#include "arrow/array/data.h"
#include "arrow/io/caching.h"
#include "arrow/util/type_fwd.h"
#include "parquet/metadata.h"  // IWYU pragma: keep
#include "parquet/platform.h"
#include "parquet/properties.h"
using arrow::ArraySpan;
using arrow::Array;
namespace parquet {

class ColumnReader;
class FileMetaData;
class PageReader;
class RowGroupMetaData;

class PARQUET_EXPORT RowGroupReader {
 public:
  // Forward declare a virtual class 'Contents' to aid dependency injection and more
  // easily create test fixtures
  // An implementation of the Contents class is defined in the .cc file
  struct Contents {
    virtual ~Contents() {}
    virtual std::unique_ptr<PageReader> GetColumnPageReader(int i) = 0;
    virtual const RowGroupMetaData* metadata() const = 0;
    virtual const ReaderProperties* properties() const = 0;
  };

  explicit RowGroupReader(std::unique_ptr<Contents> contents);

  // Returns the rowgroup metadata
  const RowGroupMetaData* metadata() const;

  // Construct a ColumnReader for the indicated row group-relative
  // column. Ownership is shared with the RowGroupReader.
  std::shared_ptr<ColumnReader> Column(int i);

  // Construct a ColumnReader, trying to enable exposed encoding.
  //
  // For dictionary encoding, currently we only support column chunks that are fully
  // dictionary encoded, i.e., all data pages in the column chunk are dictionary encoded.
  // If a column chunk uses dictionary encoding but then falls back to plain encoding, the
  // encoding will not be exposed.
  //
  // The returned column reader provides an API GetExposedEncoding() for the
  // users to check the exposed encoding and determine how to read the batches.
  //
  // \note API EXPERIMENTAL
  std::shared_ptr<ColumnReader> ColumnWithExposeEncoding(
      int i, ExposedEncoding encoding_to_expose);

  std::unique_ptr<PageReader> GetColumnPageReader(int i);

 private:
  // Holds a pointer to an instance of Contents implementation
  std::unique_ptr<Contents> contents_;
};

class PARQUET_EXPORT ParquetFileReader {
 public:
  // Declare a virtual class 'Contents' to aid dependency injection and more
  // easily create test fixtures
  // An implementation of the Contents class is defined in the .cc file
  struct PARQUET_EXPORT Contents {
    static std::unique_ptr<Contents> Open(
        std::shared_ptr<::arrow::io::RandomAccessFile> source,
        const ReaderProperties& props = default_reader_properties(),
        std::shared_ptr<FileMetaData> metadata = NULLPTR);

    static ::arrow::Future<std::unique_ptr<Contents>> OpenAsync(
        std::shared_ptr<::arrow::io::RandomAccessFile> source,
        const ReaderProperties& props = default_reader_properties(),
        std::shared_ptr<FileMetaData> metadata = NULLPTR);

    virtual ~Contents() = default;
    // Perform any cleanup associated with the file contents
    virtual void Close() = 0;
    virtual std::shared_ptr<RowGroupReader> GetRowGroup(int i) = 0;
    virtual std::shared_ptr<FileMetaData> metadata() const = 0;
  };

  ParquetFileReader();
  ~ParquetFileReader();

  // Create a file reader instance from an Arrow file object. Thread-safety is
  // the responsibility of the file implementation
  static std::unique_ptr<ParquetFileReader> Open(
      std::shared_ptr<::arrow::io::RandomAccessFile> source,
      const ReaderProperties& props = default_reader_properties(),
      std::shared_ptr<FileMetaData> metadata = NULLPTR);

  // API Convenience to open a serialized Parquet file on disk, using Arrow IO
  // interfaces.
  static std::unique_ptr<ParquetFileReader> OpenFile(
      const std::string& path, bool memory_map = false,
      const ReaderProperties& props = default_reader_properties(),
      std::shared_ptr<FileMetaData> metadata = NULLPTR);

  // Asynchronously open a file reader from an Arrow file object.
  // Does not throw - all errors are reported through the Future.
  static ::arrow::Future<std::unique_ptr<ParquetFileReader>> OpenAsync(
      std::shared_ptr<::arrow::io::RandomAccessFile> source,
      const ReaderProperties& props = default_reader_properties(),
      std::shared_ptr<FileMetaData> metadata = NULLPTR);

  void Open(std::unique_ptr<Contents> contents);
  void Close();

  // The RowGroupReader is owned by the FileReader
  std::shared_ptr<RowGroupReader> RowGroup(int i);

  // Returns the file metadata. Only one instance is ever created
  std::shared_ptr<FileMetaData> metadata() const;

  /// Pre-buffer the specified column indices in all row groups.
  ///
  /// Readers can optionally call this to cache the necessary slices
  /// of the file in-memory before deserialization. Arrow readers can
  /// automatically do this via an option. This is intended to
  /// increase performance when reading from high-latency filesystems
  /// (e.g. Amazon S3).
  ///
  /// After calling this, creating readers for row groups/column
  /// indices that were not buffered may fail. Creating multiple
  /// readers for the a subset of the buffered regions is
  /// acceptable. This may be called again to buffer a different set
  /// of row groups/columns.
  ///
  /// If memory usage is a concern, note that data will remain
  /// buffered in memory until either \a PreBuffer() is called again,
  /// or the reader itself is destructed. Reading - and buffering -
  /// only one row group at a time may be useful.
  ///
  /// This method may throw.
  void PreBuffer(const std::vector<int>& row_groups,
                 const std::vector<int>& column_indices,
                 const ::arrow::io::IOContext& ctx,
                 const ::arrow::io::CacheOptions& options);

  /// Wait for the specified row groups and column indices to be pre-buffered.
  ///
  /// After the returned Future completes, reading the specified row
  /// groups/columns will not block.
  ///
  /// PreBuffer must be called first. This method does not throw.
  ::arrow::Future<> WhenBuffered(const std::vector<int>& row_groups,
                                 const std::vector<int>& column_indices) const;

 private:
  // Holds a pointer to an instance of Contents implementation
  std::unique_ptr<Contents> contents_;
};

// Read only Parquet file metadata
std::shared_ptr<FileMetaData> PARQUET_EXPORT
ReadMetaData(const std::shared_ptr<::arrow::io::RandomAccessFile>& source);

/// \brief Scan all values in file. Useful for performance testing
/// \param[in] columns the column numbers to scan. If empty scans all
/// \param[in] column_batch_size number of values to read at a time when scanning column
/// \param[in] reader a ParquetFileReader instance
/// \return number of semantic rows in file
PARQUET_EXPORT
int64_t ScanFileContents(std::vector<int> columns, const int32_t column_batch_size,
                         ParquetFileReader* reader,uint8_t* result_data=nullptr);
PARQUET_EXPORT
int64_t ScanFileContentsArrow(std::vector<int> columns, const int32_t column_batch_size,
                         ParquetFileReader* reader, uint8_t* result_data, std::vector<std::shared_ptr<ArraySpan>>* total_array, std::vector<std::shared_ptr<ColumnReader>>* col_readers, double* compute_time);
PARQUET_EXPORT
int64_t ScanFileContentsArrowSum(std::vector<int> columns, const int32_t column_batch_size,
                         ParquetFileReader* reader, double* compute_time, std::vector<uint8_t>* btimap);                        
PARQUET_EXPORT
int64_t FilterScanFileContents(std::vector<int> columns, const int32_t column_batch_size,
                         ParquetFileReader* reader, int64_t filter_val, std::vector<uint32_t>& bitpos, bool is_gt, int64_t* filter_count, int64_t filter2, double* compute_time, int64_t base_val, parquet::Encoding::type encoding);
PARQUET_EXPORT
int64_t FilterScanFileContentsDict(std::vector<int> columns, const int32_t column_batch_size,
                         ParquetFileReader* reader, int64_t filter_val, std::vector<uint32_t>& bitpos, bool is_gt, int64_t* filter_count, int64_t filter2, double* compute_time, int64_t base_val, parquet::Encoding::type encoding);
PARQUET_EXPORT
int64_t ScanFileContentsCheck(std::vector<int> columns, const int32_t column_batch_size,
                              ParquetFileReader* reader, std::vector<int32_t>& a,
                              std::vector<int32_t>& b);
PARQUET_EXPORT
int64_t ScanFileContentsBitpos(std::vector<int> columns, const int32_t column_batch_size,
                               ParquetFileReader* reader, std::vector<uint32_t>& bitpos, uint8_t* values, double* compute_time);
PARQUET_EXPORT
int64_t ScanFileContentsBitposOld(std::vector<int> columns, const int32_t column_batch_size,
                               ParquetFileReader* reader, std::vector<uint32_t>& bitpos,
                               std::vector<int16_t>& rep_levels,
                               std::vector<int16_t>& def_levels,
                               std::vector<uint8_t>& values, double* compute_time);
PARQUET_EXPORT
int64_t ScanFileContentsBitposDict(std::vector<int> columns,
                                   const int32_t column_batch_size,
                                   ParquetFileReader* reader,
                                   std::vector<uint32_t>& bitpos, uint8_t* values, double* computetime);
}  // namespace parquet
