// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_READ_STREAMBUF_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_READ_STREAMBUF_H

#include "google/cloud/storage/internal/hash_function.h"
#include "google/cloud/storage/internal/hash_validator.h"
#include "google/cloud/storage/internal/object_read_source.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include <iostream>
#include <map>
#include <memory>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

/**
 * Defines a compilation barrier for libcurl.
 *
 * We do not want to expose the libcurl objects through `ObjectReadStream`,
 * this class abstracts away the implementation so applications are not impacted
 * by the implementation details.
 */
class ObjectReadStreambuf : public std::basic_streambuf<char> {
 public:
  ObjectReadStreambuf(ReadObjectRangeRequest const& request,
                      std::unique_ptr<ObjectReadSource> source,
                      std::streamoff pos_in_stream);

  /// Create a streambuf in a permanent error status.
  ObjectReadStreambuf(ReadObjectRangeRequest const& request, Status status);

  ~ObjectReadStreambuf() override = default;

  ObjectReadStreambuf(ObjectReadStreambuf&&) noexcept = delete;
  ObjectReadStreambuf& operator=(ObjectReadStreambuf&&) noexcept = delete;
  ObjectReadStreambuf(ObjectReadStreambuf const&) = delete;
  ObjectReadStreambuf& operator=(ObjectReadStreambuf const&) = delete;

  pos_type seekpos(pos_type pos, std::ios_base::openmode which) override;
  pos_type seekoff(off_type off, std::ios_base::seekdir dir,
                   std::ios_base::openmode which) override;

  bool IsOpen() const;
  void Close();
  Status const& status() const { return status_; }
  std::string const& received_hash() const { return received_hash_; }
  std::string const& computed_hash() const { return computed_hash_; }
  std::multimap<std::string, std::string> const& headers() const {
    return headers_;
  }

 private:
  int_type ReportError(Status status);
  void ThrowHashMismatchDelegate(char const* function_name);
  bool ValidateHashes(char const* function_name);
  bool CheckPreconditions(char const* function_name);

  int_type underflow() override;
  std::streamsize xsgetn(char* s, std::streamsize count) override;

  std::unique_ptr<ObjectReadSource> source_;
  std::streamoff source_pos_;
  std::vector<char> current_ios_buffer_;
  std::unique_ptr<HashFunction> hash_function_;
  std::unique_ptr<HashValidator> hash_validator_;
  HashValidator::Result hash_validator_result_;
  std::string computed_hash_;
  std::string received_hash_;
  Status status_;
  std::multimap<std::string, std::string> headers_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_READ_STREAMBUF_H
