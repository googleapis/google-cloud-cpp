// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_WRITE_STREAMBUF_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_WRITE_STREAMBUF_H

#include "google/cloud/storage/auto_finalize.h"
#include "google/cloud/storage/internal/hash_function.h"
#include "google/cloud/storage/internal/hash_validator.h"
#include "google/cloud/storage/internal/raw_client.h"
#include "google/cloud/storage/version.h"
#include <iostream>
#include <memory>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
class ObjectWriteStream;
namespace internal {

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

  explicit ObjectWriteStreambuf(Status status);

  ObjectWriteStreambuf(std::shared_ptr<RawClient> client,
                       ResumableUploadRequest request, std::string upload_id,
                       std::uint64_t committed_size,
                       absl::optional<ObjectMetadata> metadata,
                       std::size_t max_buffer_size,
                       std::unique_ptr<HashFunction> hash_function,
                       HashValues known_hashes,
                       std::unique_ptr<HashValidator> hash_validator,
                       AutoFinalizeConfig auto_finalize);

  ~ObjectWriteStreambuf() override = default;

  ObjectWriteStreambuf(ObjectWriteStreambuf&& rhs) = delete;
  ObjectWriteStreambuf& operator=(ObjectWriteStreambuf&& rhs) = delete;
  ObjectWriteStreambuf(ObjectWriteStreambuf const&) = delete;
  ObjectWriteStreambuf& operator=(ObjectWriteStreambuf const&) = delete;

  virtual StatusOr<QueryResumableUploadResponse> Close();
  virtual bool IsOpen() const;
  virtual bool ValidateHash(ObjectMetadata const& meta);

  virtual std::string const& received_hash() const { return received_hash_; }
  virtual std::string const& computed_hash() const { return computed_hash_; }

  /// The session id, if applicable, it is empty for non-resumable uploads.
  virtual std::string const& resumable_session_id() const { return upload_id_; }

  /// The next expected byte, if applicable, always 0 for non-resumable uploads.
  virtual std::uint64_t next_expected_byte() const { return committed_size_; }

  virtual Status last_status() const { return last_status_; }

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

  std::shared_ptr<RawClient> client_;
  ResumableUploadRequest request_;
  Status last_status_;
  std::string upload_id_;
  std::uint64_t committed_size_ = 0;
  absl::optional<ObjectMetadata> metadata_;

  std::vector<char> current_ios_buffer_;
  std::size_t max_buffer_size_;

  std::unique_ptr<HashFunction> hash_function_;
  HashValues hash_values_;
  HashValues known_hashes_;
  std::unique_ptr<HashValidator> hash_validator_;
  AutoFinalizeConfig auto_finalize_ = AutoFinalizeConfig::kDisabled;

  HashValidator::Result hash_validator_result_;
  std::string computed_hash_;
  std::string received_hash_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_WRITE_STREAMBUF_H
