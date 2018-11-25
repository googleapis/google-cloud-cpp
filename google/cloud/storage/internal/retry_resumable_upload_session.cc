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

#include "google/cloud/storage/internal/retry_resumable_upload_session.h"
#include <thread>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

namespace {
std::pair<Status, ResumableUploadResponse> ReturnError(
    Status&& last_status, RetryPolicy const& retry_policy,
    char const* error_message) {
  std::ostringstream os;
  if (retry_policy.IsExhausted()) {
    os << "Retry policy exhausted in " << error_message << ": " << last_status;
  } else {
    os << "Permanent error in " << error_message << ": " << last_status;
  }
  return std::make_pair(
      Status(last_status.status_code(), os.str(), last_status.error_details()),
      ResumableUploadResponse{});
}
}  // namespace

std::pair<Status, ResumableUploadResponse>
RetryResumableUploadSession::UploadChunk(std::string const& buffer,
                                         std::uint64_t upload_size) {
  Status last_status;
  while (not retry_policy_->IsExhausted()) {
    auto result = session_->UploadChunk(buffer, upload_size);
    if (result.first.ok()) {
      return result;
    }
    last_status = std::move(result.first);
    if (not retry_policy_->OnFailure(last_status)) {
      return ReturnError(std::move(last_status), *retry_policy_, __func__);
    }
    auto delay = backoff_policy_->OnCompletion();
    std::this_thread::sleep_for(delay);

    result = ResetSession();
    if (not result.first.ok()) {
      return result;
    }
  }
  std::ostringstream os;
  os << "Retry policy exhausted in " << __func__ << ": " << last_status;
  return std::make_pair(
      Status(last_status.status_code(), os.str(), last_status.error_details()),
      ResumableUploadResponse{});
}

std::pair<Status, ResumableUploadResponse>
RetryResumableUploadSession::ResetSession() {
  Status last_status;
  while (not retry_policy_->IsExhausted()) {
    auto result = session_->ResetSession();
    if (result.first.ok()) {
      return result;
    }
    last_status = std::move(result.first);
    if (not retry_policy_->OnFailure(last_status)) {
      return ReturnError(std::move(last_status), *retry_policy_, __func__);
    }
    auto delay = backoff_policy_->OnCompletion();
    std::this_thread::sleep_for(delay);
  }
  std::ostringstream os;
  os << "Retry policy exhausted in " << __func__ << ": " << last_status;
  return std::make_pair(
      Status(last_status.status_code(), os.str(), last_status.error_details()),
      ResumableUploadResponse{});
}

std::uint64_t RetryResumableUploadSession::next_expected_byte() const {
  return session_->next_expected_byte();
}
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
