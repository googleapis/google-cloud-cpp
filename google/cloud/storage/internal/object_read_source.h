// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_READ_SOURCE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_READ_SOURCE_H

#include "google/cloud/storage/internal/http_response.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/status_or.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * The result of reading some data from the source.
 *
 * Reading data may result in several outcomes:
 * - There was an error trying to read the data: we wrap this object in a
 *   StatusOr for this case.
 *
 * Other reads are considered successful, even if the "read" an HTTP error code.
 * Successful reads return:
 *
 * - How much of the data requested was read: returned in the `bytes_received`
 *   field.
 * - The HTTP error code for the full download. In-progress downloads have an
 *   `response.status_code == 100` (CONTINUE).
 * - At any point the call may return one or more headers, these headers are
 *   in `response.headers`.
 * - If the `response.status_code` was an error code (i.e >= 200) then the
 *   `response.payload` *may* contain additional error payload.
 */
struct ReadSourceResult {
  std::size_t bytes_received;
  HttpResponse response;
};

/**
 * A data source for ObjectReadStreambuf.
 *
 * This object represents an open download stream. It is an abstract class
 * because (a) we do not want to expose CURL types in the public headers, and
 * (b) we want to break the functionality for retry vs. simple downloads in
 * different classes.
 */
class ObjectReadSource {
 public:
  virtual ~ObjectReadSource() = default;

  virtual bool IsOpen() const = 0;

  /// Actively close a download, even if not all the data has been read.
  virtual StatusOr<HttpResponse> Close() = 0;

  /// Read more data from the download, returning any HTTP headers and error
  /// codes.
  virtual StatusOr<ReadSourceResult> Read(char* buf, std::size_t n) = 0;
};

/**
 * A ObjectReadErrorStreambufSource in a permanent error state.
 */
class ObjectReadErrorSource : public ObjectReadSource {
 public:
  explicit ObjectReadErrorSource(Status status) : status_(std::move(status)) {}

  bool IsOpen() const override { return false; }
  StatusOr<HttpResponse> Close() override { return status_; }
  StatusOr<ReadSourceResult> Read(char*, std::size_t) override {
    return status_;
  }

 private:
  Status status_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_READ_SOURCE_H
