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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_STREAMBUF_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_STREAMBUF_H

#include "google/cloud/storage/auto_finalize.h"
#include "google/cloud/storage/internal/hash_validator.h"
#include "google/cloud/storage/internal/http_response.h"
#include "google/cloud/storage/internal/object_read_source.h"
#include "google/cloud/storage/internal/resumable_upload_session.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/status_or.h"
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
class ObjectMetadata;
class ObjectWriteStream;
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
  std::streamsize xsgetn(char* s, std::streamsize count) override;

  std::unique_ptr<ObjectReadSource> source_;
  std::streamoff source_pos_;
  std::vector<char> current_ios_buffer_;
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
  ObjectWriteStreambuf() = default;

  ObjectWriteStreambuf(std::unique_ptr<ResumableUploadSession> upload_session,
                       std::size_t max_buffer_size,
                       std::unique_ptr<HashValidator> hash_validator,
                       AutoFinalizeConfig auto_finalize);

  ~ObjectWriteStreambuf() override = default;

  ObjectWriteStreambuf(ObjectWriteStreambuf&& rhs) noexcept = delete;
  ObjectWriteStreambuf& operator=(ObjectWriteStreambuf&& rhs) noexcept = delete;
  ObjectWriteStreambuf(ObjectWriteStreambuf const&) = delete;
  ObjectWriteStreambuf& operator=(ObjectWriteStreambuf const&) = delete;

  virtual StatusOr<ResumableUploadResponse> Close();
  virtual bool IsOpen() const;
  virtual bool ValidateHash(ObjectMetadata const& meta);

  virtual std::string const& received_hash() const {
    return hash_validator_result_.received;
  }
  virtual std::string const& computed_hash() const {
    return hash_validator_result_.computed;
  }

  /// The session id, if applicable, it is empty for non-resumable uploads.
  virtual std::string const& resumable_session_id() const {
    return upload_session_->session_id();
  }

  /// The next expected byte, if applicable, always 0 for non-resumable uploads.
  virtual std::uint64_t next_expected_byte() const {
    return upload_session_->next_expected_byte();
  }

  virtual Status last_status() const { return last_response_.status(); }

 protected:
  int sync() override;
  std::streamsize xsputn(char const* s, std::streamsize count) override;
  int_type overflow(int_type ch) override;

 private:
  friend class google::cloud::storage::ObjectWriteStream;
  /**
   * Automatically finalize the upload unless configured to not do so.
   *
   * Called by the ObjectWriteStream destructor, some applications prefer to
   * explicitly finalize an upload. For example, they may start an upload,
   * checkpoint the upload id, then upload in chunks and may *not* want to
   * finalize the upload in the presence of exceptions that destroy any
   * ObjectWriteStream.
   */
  void AutoFlushFinal();

  /// Flush any data if possible.
  void Flush();

  /// Flush any remaining data and finalize the upload.
  void FlushFinal();

  /// Upload a round chunk
  void FlushRoundChunk(ConstBufferSequence buffers);

  /// The current used bytes in the put area (aka current_ios_buffer_)
  std::size_t put_area_size() const { return pptr() - pbase(); }

  std::unique_ptr<ResumableUploadSession> upload_session_;

  std::vector<char> current_ios_buffer_;
  std::size_t max_buffer_size_;

  std::unique_ptr<HashValidator> hash_validator_;
  AutoFinalizeConfig auto_finalize_ = AutoFinalizeConfig::kDisabled;

  HashValidator::Result hash_validator_result_;

  StatusOr<ResumableUploadResponse> last_response_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_STREAMBUF_H
