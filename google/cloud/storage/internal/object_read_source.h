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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_READ_SOURCE_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_READ_SOURCE_H_

#include "google/cloud/status_or.h"
#include "google/cloud/storage/internal/http_response.h"
#include "google/cloud/storage/version.h"
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
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
  virtual StatusOr<HttpResponse> Close() = 0;

  virtual StatusOr<HttpResponse> Read(std::string& buffer) = 0;
};

/**
 * A ObjectReadErrorStreambufSource in a permanent error state.
 */
class ObjectReadErrorSource : public ObjectReadSource {
 public:
  explicit ObjectReadErrorSource(Status status) : status_(std::move(status)) {}

  bool IsOpen() const override { return false; }
  StatusOr<HttpResponse> Close() override { return status_; }
  StatusOr<HttpResponse> Read(std::string& buffer) override { return status_; }

 private:
  Status status_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_READ_SOURCE_H_
