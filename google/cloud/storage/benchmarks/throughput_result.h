// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_THROUGHPUT_RESULT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_THROUGHPUT_RESULT_H

#include "google/cloud/storage/benchmarks/benchmark_utils.h"
#include <chrono>
#include <cstdint>

namespace google {
namespace cloud {
namespace storage_benchmarks {

/// The operation used for the experiment
enum OpType {
  /// The experiment performed a resumable upload, using  Client::WriteObject()
  /// or an equivalent function.
  kOpWrite,
  /// The experiment performed a simple upload, using Client::InsertObject() or
  /// an
  /// equivalent function.
  kOpInsert,
  /// The experiment performed a downlad, using Client::InsertObject() or an
  /// equivalent function.
  /// This was the first download of this object in the experiment.
  // TODO(#4350) - use a separate field to count downloads / uploads
  kOpRead0,
  /// The experiment performed a downlad, using Client::InsertObject() or an
  /// equivalent function.
  /// This was the second download of this object in the experiment.
  kOpRead1,
  /// The experiment performed a downlad, using Client::InsertObject() or an
  /// equivalent function.
  /// This was the third download of this object in the experiment.
  kOpRead2,
};

/**
 * The result of running a throughput benchmark iteration.
 *
 * The benchmarks in this directory run the same "experiment" with different
 * conditions, downloading the same GCS object N times, or uploading objects
 * with different buffer sizes. This struct is used to represent the conditions
 * used in the experiment (buffer sizes, object size, checksum settings, API,
 * etc.) as well as its results: status, CPU time, and elapsed time.
 */
struct ThroughputResult {
  /// The type of operation in this experiment.
  OpType op;
  /// The total size of the object involved in this experiment. Currently also
  /// represents the number of bytes transferred.
  // TODO(#4349) - use a separate field to represent the bytes transferred
  std::int64_t object_size;
  /// The size of the application buffer (for .read() or .write() calls).
  std::size_t app_buffer_size;
  /// The size of the library buffers (if any).
  std::size_t lib_buffer_size;
  /// True if the CRC32C checksums are enabled in this experiment.
  bool crc_enabled;
  /// True if the MD5 hashes are enabled in this experiment.
  bool md5_enabled;
  /// The API, protocol, or library used in this experiment.
  ApiName api;
  /// The total time used to complete the experiment.
  std::chrono::microseconds elapsed_time;
  /// The amount of CPU time (as reported by getrusage(2)) consumed in the
  /// experiment.
  std::chrono::microseconds cpu_time;
  /// The result of the operation. The analysis may need to discard failed
  /// uploads or downloads.
  google::cloud::Status status;
};

/// Print @p r as a CSV line.
void PrintAsCsv(std::ostream& os, ThroughputResult const& r);

/// Print the field names produced by `PrintAsCsv()`
void PrintThroughputResultHeader(std::ostream& os);

char const* ToString(OpType op);

}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_THROUGHPUT_RESULT_H
