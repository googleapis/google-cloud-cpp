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

#include "google/cloud/status_or.h"
#include "google/cloud/storage/internal/hash_validator.h"
#include "google/cloud/storage/internal/http_response.h"
#include "google/cloud/storage/internal/object_read_source.h"
#include "google/cloud/storage/version.h"
#include <iostream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
class ObjectMetadata;
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
                      std::unique_ptr<ObjectReadSource> source);

  /// Create a streambuf in a permanent error status.
  ObjectReadStreambuf(ReadObjectRangeRequest const& request, Status status);

  ~ObjectReadStreambuf() override = default;

  ObjectReadStreambuf(ObjectReadStreambuf&&) noexcept = delete;
  ObjectReadStreambuf& operator=(ObjectReadStreambuf&&) noexcept = delete;
  ObjectReadStreambuf(ObjectReadStreambuf const&) = delete;
  ObjectReadStreambuf& operator=(ObjectReadStreambuf const&) = delete;

  bool IsOpen() const;
  void Close();
  Status const& status() const { return status_; }
  std::string const& received_hash() const {
    return hash_validator_result_.received;
  }
  std::string const& computed_hash() const {
    return hash_validator_result_.computed;
  }
  std::multimap<std::string, std::string> const& headers() const {
    return headers_;
  }

 private:
  int_type ReportError(Status status);
  void SetEmptyRegion();
  StatusOr<int_type> Peek();

  int_type underflow() override;

  std::unique_ptr<ObjectReadSource> source_;
  std::string current_ios_buffer_;
  std::unique_ptr<HashValidator> hash_validator_;
  HashValidator::Result hash_validator_result_;
  Status status_;
  std::multimap<std::string, std::string> headers_;
};

/**
 * Defines a compilation barrier for libcurl.
 *
 * We do not want to expose the libcurl objects through `ObjectWriteStream`,
 * this class abstracts away the implementation so applications are not impacted
 * by the implementation details.
 */
class ObjectWriteStreambuf : public std::basic_streambuf<char> {
 public:
  ObjectWriteStreambuf() : std::basic_streambuf<char>() {}
  ~ObjectWriteStreambuf() override = default;

  ObjectWriteStreambuf(ObjectWriteStreambuf&& rhs) noexcept = delete;
  ObjectWriteStreambuf& operator=(ObjectWriteStreambuf&& rhs) noexcept = delete;
  ObjectWriteStreambuf(ObjectWriteStreambuf const&) = delete;
  ObjectWriteStreambuf& operator=(ObjectWriteStreambuf const&) = delete;

  StatusOr<HttpResponse> Close();
  virtual bool IsOpen() const = 0;
  virtual bool ValidateHash(ObjectMetadata const& meta) = 0;
  virtual std::string const& received_hash() const = 0;
  virtual std::string const& computed_hash() const = 0;

  /// The session id, if applicable, it is empty for non-resumable uploads.
  virtual std::string const& resumable_session_id() const = 0;

  /// The next expected byte, if applicable, always 0 for non-resumable uploads.
  virtual std::uint64_t next_expected_byte() const = 0;

 protected:
  virtual StatusOr<HttpResponse> DoClose() = 0;
};

/**
 * A write stream in a permanent error state.
 */
class ObjectWriteErrorStreambuf : public ObjectWriteStreambuf {
 public:
  explicit ObjectWriteErrorStreambuf(Status status)
      : status_(std::move(status)) {}

  bool IsOpen() const override { return is_open_; }
  bool ValidateHash(ObjectMetadata const& meta) override { return false; }
  std::string const& received_hash() const override { return string_; }
  std::string const& computed_hash() const override { return string_; }
  std::string const& resumable_session_id() const override { return string_; }
  std::uint64_t next_expected_byte() const override { return 0; }

 protected:
  StatusOr<HttpResponse> DoClose() override {
    is_open_ = false;
    return status_;
  }

 private:
  bool is_open_ = true;
  Status status_;
  std::string string_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_STREAMBUF_H_
