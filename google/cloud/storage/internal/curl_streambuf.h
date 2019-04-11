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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_STREAMBUF_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_STREAMBUF_H_

#include "google/cloud/storage/internal/curl_download_request.h"
#include "google/cloud/storage/internal/hash_validator.h"
#include "google/cloud/storage/internal/object_streambuf.h"
#include "google/cloud/storage/version.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Makes streaming download requests using libcurl.
 */
class CurlReadStreambuf : public ObjectReadStreambuf {
 public:
  explicit CurlReadStreambuf(CurlDownloadRequest&& download,
                             std::size_t target_buffer_size,
                             std::unique_ptr<HashValidator> hash_validator);

  ~CurlReadStreambuf() override = default;

  void Close() override;
  bool IsOpen() const override;
  StatusOr<int_type> Peek();

  Status const& status() const override { return status_; }
  std::string const& received_hash() const override {
    return hash_validator_result_.received;
  }
  std::string const& computed_hash() const override {
    return hash_validator_result_.computed;
  }
  std::multimap<std::string, std::string> const& headers() const override {
    return headers_;
  }

 protected:
  int_type underflow() override;

  int_type ReportError(Status status);

  void SetEmptyRegion();

 private:
  CurlDownloadRequest download_;
  std::string current_ios_buffer_;
  std::size_t target_buffer_size_;

  std::unique_ptr<HashValidator> hash_validator_;
  HashValidator::Result hash_validator_result_;
  Status status_;
  std::multimap<std::string, std::string> headers_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_STREAMBUF_H_
