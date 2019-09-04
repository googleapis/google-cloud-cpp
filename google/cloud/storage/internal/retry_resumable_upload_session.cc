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
    os << "Retry policy exhausted in " << error_message << ": " << last_status;
  } else {
    os << "Permanent error in " << error_message << ": " << last_status;
  }
  return Status(last_status.code(), os.str());
}
}  // namespace

StatusOr<ResumableUploadResponse> RetryResumableUploadSession::UploadChunk(
    std::string const& buffer) {
  return UploadGenericChunk(buffer, optional<std::uint64_t>());
}

StatusOr<ResumableUploadResponse> RetryResumableUploadSession::UploadFinalChunk(
    std::string const& buffer, std::uint64_t upload_size) {
  return UploadGenericChunk(buffer, upload_size);
}

StatusOr<ResumableUploadResponse>
RetryResumableUploadSession::UploadGenericChunk(
    std::string const& buffer, optional<std::uint64_t> const& upload_size) {
  bool const is_final_chunk = upload_size.has_value();
  char const* const func = is_final_chunk ? "UploadFinalChunk" : "UploadChunk";
  std::uint64_t next_byte = session_->next_expected_byte();
  Status last_status(StatusCode::kDeadlineExceeded,
                     "Retry policy exhausted before first attempt was made.");
  // On occasion, we might need to retry uploading only a part of the buffer.
  // The current APIs require us to copy the buffer in such a scenario. We can
  // and want to avoid the copy in the common case, so we use this variable to
  // either reference the copy or the original buffer.
  // TODO(#3036): change the APIs to avoid this extra copy.
  std::string const* buffer_to_use = &buffer;
  std::string truncated_buffer;
  while (!retry_policy_->IsExhausted()) {
    std::uint64_t new_next_byte = session_->next_expected_byte();
    if (new_next_byte < next_byte) {
      std::stringstream os;
      os << func << ": server has not confirmed bytes which we no longer hold ("
         << new_next_byte << "-" << next_byte
         << "). This is most likely a bug in the GCS client. Please report it "
            "at https://github.com/googleapis/google-cloud-cpp/issues/new";
      return Status(StatusCode::kInternal, os.str());
    }
    if (new_next_byte > next_byte) {
      truncated_buffer = buffer_to_use->substr(new_next_byte - next_byte);
      buffer_to_use = &truncated_buffer;
      next_byte = new_next_byte;
    }
    auto result = is_final_chunk
                      ? session_->UploadFinalChunk(*buffer_to_use, *upload_size)
                      : session_->UploadChunk(*buffer_to_use);
    if (result.ok()) {
      if (is_final_chunk &&
          result->upload_state == ResumableUploadResponse::kDone) {
        // If it's a final chunk and it succeded, return.
        return result;
      }
      if (next_expected_byte() - next_byte == buffer_to_use->size()) {
        // Otherwise, return only if there were no failures and it wasn't a
        // short write.
        return result;
      }
      std::stringstream os;
      os << "Short write. Previous next_byte=" << next_byte
         << ", current next_byte=" << next_expected_byte()
         << ", inteded to write " << buffer_to_use->size();
      last_status = Status(StatusCode::kUnavailable, os.str());
      // Don't reset the session on a short write nor wait according to the
      // backoff policy - we did get a response from the server after all.
      continue;
    }
    last_status = std::move(result).status();
    if (!retry_policy_->OnFailure(last_status)) {
      return ReturnError(std::move(last_status), *retry_policy_, __func__);
    }
    auto delay = backoff_policy_->OnCompletion();
    std::this_thread::sleep_for(delay);

    result = ResetSession();
    if (!result.ok()) {
      return result;
    }
  }
  std::ostringstream os;
  os << "Retry policy exhausted in " << func << ": " << last_status;
  return Status(last_status.code(), os.str());
}

StatusOr<ResumableUploadResponse> RetryResumableUploadSession::ResetSession() {
  Status last_status(StatusCode::kDeadlineExceeded,
                     "Retry policy exhausted before first attempt was made.");
  while (!retry_policy_->IsExhausted()) {
    auto result = session_->ResetSession();
    if (result.ok()) {
      return result;
    }
    last_status = std::move(result).status();
    if (!retry_policy_->OnFailure(last_status)) {
      return ReturnError(std::move(last_status), *retry_policy_, __func__);
    }
    auto delay = backoff_policy_->OnCompletion();
    std::this_thread::sleep_for(delay);
  }
  std::ostringstream os;
  os << "Retry policy exhausted in " << __func__ << ": " << last_status;
  return Status(last_status.code(), os.str());
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
