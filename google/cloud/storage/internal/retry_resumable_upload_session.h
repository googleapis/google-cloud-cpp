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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RETRY_RESUMABLE_UPLOAD_SESSION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RETRY_RESUMABLE_UPLOAD_SESSION_H

#include "google/cloud/storage/internal/resumable_upload_session.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/version.h"
#include "absl/functional/function_ref.h"
#include "absl/types/optional.h"
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
/**
 * Decorates a `ResumableUploadSession` to retry operations that fail.
 *
 * Note that to retry some operations the session may need to query the current
 * upload status.
 */
class RetryResumableUploadSession : public ResumableUploadSession {
 public:
  explicit RetryResumableUploadSession(
      std::unique_ptr<ResumableUploadSession> session,
      std::unique_ptr<RetryPolicy> retry_policy,
      std::unique_ptr<BackoffPolicy> backoff_policy)
      : session_(std::move(session)),
        retry_policy_prototype_(std::move(retry_policy)),
        backoff_policy_prototype_(std::move(backoff_policy)) {}

  StatusOr<ResumableUploadResponse> UploadChunk(
      ConstBufferSequence const& buffers) override;
  StatusOr<ResumableUploadResponse> UploadFinalChunk(
      ConstBufferSequence const& buffers, std::uint64_t upload_size,
      HashValues const& full_object_hashes) override;
  StatusOr<ResumableUploadResponse> ResetSession() override;
  std::uint64_t next_expected_byte() const override;
  std::string const& session_id() const override;
  bool done() const override;
  StatusOr<ResumableUploadResponse> const& last_response() const override;

 private:
  using UploadChunkFunction =
      absl::FunctionRef<StatusOr<ResumableUploadResponse>(
          ConstBufferSequence const&)>;
  // Retry either UploadChunk or either UploadFinalChunk. Note that we need a
  // copy of the buffers because on some retries they need to be modified.
  StatusOr<ResumableUploadResponse> UploadGenericChunk(
      char const* caller, ConstBufferSequence buffers,
      UploadChunkFunction upload);

  // Reset the current session using previously cloned policies.
  StatusOr<ResumableUploadResponse> ResetSession(RetryPolicy& retry_policy,
                                                 BackoffPolicy& backoff_policy,
                                                 Status last_status);

  void AppendDebug(char const* action, std::uint64_t value);

  struct DebugEntry {
    std::string action;
    std::uint64_t value;
    std::thread::id tid_;
  };

  std::unique_ptr<ResumableUploadSession> session_;
  std::unique_ptr<RetryPolicy const> retry_policy_prototype_;
  std::unique_ptr<BackoffPolicy const> backoff_policy_prototype_;
  std::mutex mu_;
  std::deque<DebugEntry> debug_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RETRY_RESUMABLE_UPLOAD_SESSION_H
