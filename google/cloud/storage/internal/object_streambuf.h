// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_STREAMBUF_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_STREAMBUF_H_

#include "google/cloud/storage/internal/http_response.h"
#include <iostream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * A `std::basic_streambuf` that writes to a GCS Object.
 */
class ObjectWriteStreamBuf : public std::basic_streambuf<char> {
 public:
  ObjectWriteStreamBuf() : std::basic_streambuf<char>() {}
  ~ObjectWriteStreamBuf() override = default;

  ObjectWriteStreamBuf(ObjectWriteStreamBuf&& rhs) noexcept = delete;
  ObjectWriteStreamBuf& operator=(ObjectWriteStreamBuf&& rhs) noexcept = delete;
  ObjectWriteStreamBuf(ObjectWriteStreamBuf const&) = delete;
  ObjectWriteStreamBuf& operator=(ObjectWriteStreamBuf const&) = delete;

  HttpResponse Close();
  virtual bool IsOpen() const = 0;

 protected:
  virtual HttpResponse DoClose() = 0;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_STREAMBUF_H_
