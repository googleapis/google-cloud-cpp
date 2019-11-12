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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RETRY_RESUMABLE_UPLOAD_SESSION_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RETRY_RESUMABLE_UPLOAD_SESSION_H_

#include "google/cloud/storage/internal/resumable_upload_session.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/version.h"
#include <memory>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
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
      std::string const& buffer) override;
  StatusOr<ResumableUploadResponse> UploadFinalChunk(
      std::string const& buffer, std::uint64_t upload_size) override;
  StatusOr<ResumableUploadResponse> ResetSession() override;
  std::uint64_t next_expected_byte() const override;
  std::string const& session_id() const override;
  bool done() const override;
  StatusOr<ResumableUploadResponse> const& last_response() const override;

 private:
  // Retry either UploadChunk or either UploadFinalChunk.
  StatusOr<ResumableUploadResponse> UploadGenericChunk(
      std::string const& buffer, optional<std::uint64_t> const& upload_size);

  // Reset the current session using previously cloned policies.
  StatusOr<ResumableUploadResponse> ResetSession(RetryPolicy& retry_policy,
                                                 BackoffPolicy& backoff_policy,
                                                 Status last_status);

  std::unique_ptr<ResumableUploadSession> session_;
  std::unique_ptr<RetryPolicy const> retry_policy_prototype_;
  std::unique_ptr<BackoffPolicy const> backoff_policy_prototype_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RETRY_RESUMABLE_UPLOAD_SESSION_H_
