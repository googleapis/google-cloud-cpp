// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_BUFFER_READ_OBJECT_DATA_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_BUFFER_READ_OBJECT_DATA_H

#include "google/cloud/version.h"
#include "absl/strings/cord.h"
#include <string>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Buffer data received from `ReadObject()`.
 *
 * The client library API to download objects keeps the download open until all
 * the data is received (or the download is interrupted by the application).
 * While downloading, the application requests fixed amounts of data, which may
 * be smaller than the amount of data received in a
 * `google.storage.v2.ReadObjectResponse` message. This class buffers any
 * excess data until the application requests more.
 *
 * It may be possible to further optimize this class by using `std::string` and
 * `absl::string_view` if `g.c.s.v2.ChecksummedData` does not use `absl::Cord`.
 * We expect that case to be rare in the near future, and while it would save
 * some allocations, it will not save any data copies. The data copies are
 * shown to be more expensive in our experiments.
 */
class GrpcBufferReadObjectData {
 public:
  GrpcBufferReadObjectData() = default;

  /// Fill @p buffer from the internal buffers.
  std::size_t FillBuffer(char* buffer, std::size_t n);

  /**
   * Save @p contents in the internal buffers and use them to fill @p buffer.
   *
   * This overload is used when Protobuf does **not** implement `[ctype = CORD]`
   * or if the storage service proto files lack this annotation.
   */
  std::size_t HandleResponse(char* buffer, std::size_t n, std::string contents);

  /**
   * Save @p contents in the internal buffers and use them to fill @p buffer.
   *
   * This overload is used when Protobuf implements `[ctype = CORD]` and the
   * storage service proto files use this annotation.
   */
  std::size_t HandleResponse(char* buffer, std::size_t n, absl::Cord contents);

 private:
  absl::Cord contents_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_BUFFER_READ_OBJECT_DATA_H
