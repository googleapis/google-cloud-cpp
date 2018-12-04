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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_RESUMABLE_STREAMBUF_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_RESUMABLE_STREAMBUF_H_

#include "google/cloud/storage/internal/hash_validator.h"
#include "google/cloud/storage/internal/object_streambuf.h"
#include "google/cloud/storage/internal/raw_client.h"
#include <iostream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
class ObjectMetadata;
namespace internal {
/**
 * Implement a wrapper for libcurl-based resumable uploads.
 */
class CurlResumableStreambuf : public ObjectWriteStreambuf {
 public:
  explicit CurlResumableStreambuf(
      std::unique_ptr<ResumableUploadSession> upload_session,
      std::size_t max_buffer_size,
      std::unique_ptr<HashValidator> hash_validator);

  ~CurlResumableStreambuf() override = default;

  bool IsOpen() const override;
  void ValidateHash(ObjectMetadata const& meta) override;
  std::string const& received_hash() const override {
    return hash_validator_result_.received;
  }
  std::string const& computed_hash() const override {
    return hash_validator_result_.computed;
  }

 protected:
  int sync() override;
  std::streamsize xsputn(char const* s, std::streamsize count) override;
  int_type overflow(int_type ch) override;
  // TODO(coryan) this is an ugly return type.
  HttpResponse DoClose() override;

 private:
  /// Raise an exception if the stream is closed.
  void Validate(char const* where) const;

  /// Flush the libcurl buffer and swap it with the iostream buffer.
  HttpResponse Flush(bool final_chunk);

  std::unique_ptr<ResumableUploadSession> upload_session_;

  std::string current_ios_buffer_;
  std::size_t max_buffer_size_;

  std::unique_ptr<HashValidator> hash_validator_;
  HashValidator::Result hash_validator_result_;

  HttpResponse last_response_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_RESUMABLE_STREAMBUF_H_
