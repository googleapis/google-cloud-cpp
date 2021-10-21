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
#include <sstream>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

namespace {
StatusOr<ResumableUploadResponse> ReturnError(Status&& last_status,
                                              RetryPolicy const& retry_policy,
                                              char const* error_message) {
  std::ostringstream os;
  if (retry_policy.IsExhausted()) {
    os << "Retry policy exhausted in " << error_message << ": "
       << last_status.message();
  } else {
    os << "Permanent error in " << error_message << ": "
       << last_status.message();
  }
  return Status(last_status.code(), std::move(os).str());
}
}  // namespace

StatusOr<ResumableUploadResponse> RetryResumableUploadSession::UploadChunk(
    ConstBufferSequence const& buffers) {
  return UploadGenericChunk(buffers, absl::optional<std::uint64_t>());
}

StatusOr<ResumableUploadResponse> RetryResumableUploadSession::UploadFinalChunk(
    ConstBufferSequence const& buffers, std::uint64_t upload_size) {
  return UploadGenericChunk(buffers, upload_size);
}

StatusOr<ResumableUploadResponse>
RetryResumableUploadSession::UploadGenericChunk(
    ConstBufferSequence buffers,
    absl::optional<std::uint64_t> const& upload_size) {
  bool const is_final_chunk = upload_size.has_value();
  char const* const func = is_final_chunk ? "UploadFinalChunk" : "UploadChunk";
  std::uint64_t next_byte = session_->next_expected_byte();
  Status last_status(StatusCode::kDeadlineExceeded,
                     "Retry policy exhausted before first attempt was made.");
  // On occasion, we might need to retry uploading only a part of the buffer.
  // The current APIs require us to copy the buffer in such a scenario. We can
  // and want to avoid the copy in the common case, so we use this variable to
  // either reference the copy or the original buffer.

  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  while (!retry_policy->IsExhausted()) {
    std::uint64_t new_next_byte = session_->next_expected_byte();
    if (new_next_byte < next_byte) {
      std::stringstream os;
      os << func << ": server previously confirmed " << next_byte
         << " bytes as committed, but the current response only reports "
         << new_next_byte << " bytes as committed."
         << " This is most likely a bug in the GCS client library, possibly"
         << " related to parsing the server response."
         << " Please report it at"
         << " https://github.com/googleapis/google-cloud-cpp/issues/new"
         << "    Include as much information as you can including this message";
      auto const& last_response = session_->last_response();
      if (!last_response) {
        os << ", last_response.status=" << last_response.status();
      } else {
        os << ", last_response.value=" << last_response.value();
      }
      return Status(StatusCode::kInternal, os.str());
    }
    if (new_next_byte > next_byte) {
      PopFrontBytes(buffers,
                    static_cast<std::size_t>(new_next_byte - next_byte));
      next_byte = new_next_byte;
    }
    auto result = is_final_chunk
                      ? session_->UploadFinalChunk(buffers, *upload_size)
                      : session_->UploadChunk(buffers);
    if (result.ok()) {
      if (result->upload_state == ResumableUploadResponse::kDone) {
        // The upload was completed. This can happen even if
        // `is_final_chunk == false`, for example, if the application includes
        // the X-Upload-Content-Length` header, which allows the server to
        // detect a completed upload "early".
        return result;
      }
      auto current_next_expected_byte = next_expected_byte();
      auto const total_bytes = TotalBytes(buffers);
      if (current_next_expected_byte - next_byte == total_bytes) {
        // Otherwise, return only if there were no failures and it wasn't a
        // short write.
        return result;
      }
      std::stringstream os;
      os << "Short write. Previous next_byte=" << next_byte
         << ", current next_byte=" << current_next_expected_byte
         << ", intended to write=" << total_bytes
         << ", wrote=" << current_next_expected_byte - next_byte;
      last_status = Status(StatusCode::kUnavailable, os.str());
      // Don't reset the session on a short write nor wait according to the
      // backoff policy - we did get a response from the server after all.
      continue;
    }
    last_status = std::move(result).status();
    if (!retry_policy->OnFailure(last_status)) {
      return ReturnError(std::move(last_status), *retry_policy, __func__);
    }
    auto delay = backoff_policy->OnCompletion();
    std::this_thread::sleep_for(delay);

    result =
        ResetSession(*retry_policy, *backoff_policy, std::move(last_status));
    if (!result.ok()) {
      return result;
    }
    last_status = Status();
  }
  std::ostringstream os;
  os << "Retry policy exhausted in " << func << ": " << last_status.message();
  return Status(last_status.code(), std::move(os).str());
}

StatusOr<ResumableUploadResponse> RetryResumableUploadSession::ResetSession(
    RetryPolicy& retry_policy, BackoffPolicy& backoff_policy,
    Status last_status) {
  while (!retry_policy.IsExhausted()) {
    auto result = session_->ResetSession();
    if (result.ok()) {
      return result;
    }
    last_status = std::move(result).status();
    if (!retry_policy.OnFailure(last_status)) {
      return ReturnError(std::move(last_status), retry_policy, __func__);
    }
    auto delay = backoff_policy.OnCompletion();
    std::this_thread::sleep_for(delay);
  }
  std::ostringstream os;
  os << "Retry policy exhausted in " << __func__ << ": "
     << last_status.message();
  return Status(last_status.code(), std::move(os).str());
}

StatusOr<ResumableUploadResponse> RetryResumableUploadSession::ResetSession() {
  Status last_status(StatusCode::kDeadlineExceeded,
                     "Retry policy exhausted before first attempt was made.");
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  return ResetSession(*retry_policy, *backoff_policy, std::move(last_status));
}

std::uint64_t RetryResumableUploadSession::next_expected_byte() const {
  return session_->next_expected_byte();
}

std::string const& RetryResumableUploadSession::session_id() const {
  return session_->session_id();
}

bool RetryResumableUploadSession::done() const { return session_->done(); }

StatusOr<ResumableUploadResponse> const&
RetryResumableUploadSession::last_response() const {
  return session_->last_response();
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
