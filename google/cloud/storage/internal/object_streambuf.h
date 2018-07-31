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
 * A `std::basic_streambuf` to read from a GCS Object.
 */
class ObjectReadStreambuf : public std::basic_streambuf<char> {
 public:
  ObjectReadStreambuf() : std::basic_streambuf<char>() {}
  ~ObjectReadStreambuf() override = default;

  ObjectReadStreambuf(ObjectReadStreambuf&&) noexcept = delete;
  ObjectReadStreambuf& operator=(ObjectReadStreambuf&&) noexcept = delete;
  ObjectReadStreambuf(ObjectReadStreambuf const&) = delete;
  ObjectReadStreambuf& operator=(ObjectReadStreambuf const&) = delete;

  virtual HttpResponse Close() = 0;
  virtual bool IsOpen() const = 0;
};

/**
 * A `std::basic_streambuf` that writes to a GCS Object.
 */
class ObjectWriteStreambuf : public std::basic_streambuf<char> {
 public:
  ObjectWriteStreambuf() : std::basic_streambuf<char>() {}
  ~ObjectWriteStreambuf() override = default;

  ObjectWriteStreambuf(ObjectWriteStreambuf&& rhs) noexcept = delete;
  ObjectWriteStreambuf& operator=(ObjectWriteStreambuf&& rhs) noexcept = delete;
  ObjectWriteStreambuf(ObjectWriteStreambuf const&) = delete;
  ObjectWriteStreambuf& operator=(ObjectWriteStreambuf const&) = delete;

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
