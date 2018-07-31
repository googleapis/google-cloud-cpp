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
#include "google/cloud/storage/internal/curl_upload_request.h"
#include "google/cloud/storage/internal/object_streambuf.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Implement a wrapper for libcurl-based streaming downloads.
 */
class CurlReadStreambuf : public ObjectReadStreambuf {
 public:
  explicit CurlReadStreambuf(CurlDownloadRequest&& download,
                             std::size_t target_buffer_size);

  ~CurlReadStreambuf() override = default;

  HttpResponse Close() override;
  bool IsOpen() const override;

 protected:
  int_type underflow() override;

 private:
  CurlDownloadRequest download_;
  std::string current_ios_buffer_;
  std::size_t target_buffer_size_;
};

/**
 * Implement a wrapper for libcurl-based streaming uploads.
 */
class CurlStreambuf : public ObjectWriteStreambuf {
 public:
  explicit CurlStreambuf(CurlUploadRequest&& upload,
                         std::size_t max_buffer_size);

  ~CurlStreambuf() override = default;

  bool IsOpen() const override;

 protected:
  int sync() override;
  std::streamsize xsputn(char const* s, std::streamsize count) override;
  int_type overflow(int_type ch) override;
  HttpResponse DoClose() override;

 private:
  /// Raise an exception if the stream is closed.
  void Validate(char const* where) const;

  /// Flush the libcurl buffer and swap it with the iostream buffer.
  void SwapBuffers();

  CurlUploadRequest upload_;
  std::string current_ios_buffer_;
  std::size_t max_buffer_size_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_STREAMBUF_H_
