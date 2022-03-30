// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/retry_resumable_upload_session.h"
#include <deque>
#include <sstream>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

namespace {
StatusOr<ResumableUploadResponse> ReturnError(Status const& last_status,
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

RetryResumableUploadSession::RetryResumableUploadSession(
    std::unique_ptr<ResumableUploadSession> session,
    std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy)
    : session_(std::move(session)),
      retry_policy_prototype_(std::move(retry_policy)),
      backoff_policy_prototype_(std::move(backoff_policy)) {}

RetryResumableUploadSession::RetryResumableUploadSession(
    std::unique_ptr<ResumableUploadSession> session,
    std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy,
    ResumableUploadResponse const& last_response)
    : session_(std::move(session)),
      committed_size_(last_response.committed_size.value_or(0)),
      retry_policy_prototype_(std::move(retry_policy)),
      backoff_policy_prototype_(std::move(backoff_policy)) {}

StatusOr<ResumableUploadResponse> RetryResumableUploadSession::UploadChunk(
    ConstBufferSequence const& buffers) {
  return UploadGenericChunk(__func__, buffers,
                            [this](ConstBufferSequence const& b) {
                              return session_->UploadChunk(b);
                            });
}

StatusOr<ResumableUploadResponse> RetryResumableUploadSession::UploadFinalChunk(
    ConstBufferSequence const& buffers, std::uint64_t upload_size,
    HashValues const& hashes) {
  return UploadGenericChunk(
      __func__, buffers,
      [this, upload_size, hashes](ConstBufferSequence const& b) {
        return session_->UploadFinalChunk(b, upload_size, hashes);
      });
}

// Implements the retry loop for a resumable upload session.
//
// A description of resumable uploads can be found at:
//     https://cloud.google.com/storage/docs/performing-resumable-uploads
//
// A description of the gRPC analog can be found in the proto file. Pay
// particular attention to the documentation for `WriteObject()`,
// `WriteObjectRequest`, `StartResumablewrite()` and `QueryResumableWrite()`:
//    https://github.com/googleapis/googleapis/blob/master/google/storage/v2/storage.proto
//
// At a high level one starts a resumable upload by creating a "session". These
// sessions are persistent (they survive disconnections from the service). One
// can even resume uploads after shutting down and restarting an application.
// Their current state can be queried using a simple RPC (or a PUT request
// without payload).
//
// Resumable uploads make progress by sending "chunks", either a single PUT
// request in REST-based transports, or a client-side streaming RPC for
// gRPC-based transports.
//
// Resumable uploads complete when the application sends the last bytes of the
// object. In the client library we mostly start uploads without knowing the
// number of bytes until a "final" chunk.  In this final chunk we set the
// `Content-Range:` header to the `bytes X-N/N` format (there is an equivalent
// form in gRPC).  In some cases the application can short-circuit this by
// setting the X-Upload-Content-Length header when the upload is created.
//
// When a chunk upload fails the application should query the state of the
// session before continuing.
//
// There are a couple of subtle cases:
// - A chunk uploads can "succeed", but report that 0 bytes were committed,
//   or not report how many bytes were committed.  The application should
//   query the state of the upload in this case:
//       https://cloud.google.com/storage/docs/performing-resumable-uploads#status-check
//   > If Cloud Storage has not yet persisted any bytes, the 308 response does
//   > **not have a Range header**. In this case, you should start your upload
//   > from the beginning.
// - A chunk upload can partially succeed, in this case the application should
//   resend the remaining bytes.
// - Resending already persisted bytes is safe:
//       https://cloud.google.com/storage/docs/performing-resumable-uploads#resume-upload
//   > Cloud Storage ignores any bytes you send at an offset that
//   > Cloud Storage has already persisted.
//
// In summary, after a failed upload operation the retry loop may need to query
// the status of the session before uploading more data. Note that the query
// operations themselves may fail with transients, and thus need to be performed
// as part of the retry loop.
//
// To simplify the loop we keep a pointer to the current "operation" that the
// retry loop is trying to get to succeed. First we try an upload, if that
// fails (a transient failure, or a 0-committed-bytes success) we switch to
// trying the ResetSession() operation until it succeeds, at which point we
// can start the upload operations again.
//
StatusOr<ResumableUploadResponse>
RetryResumableUploadSession::UploadGenericChunk(char const* caller,
                                                ConstBufferSequence buffers,
                                                UploadChunkFunction upload) {
  Status last_status(StatusCode::kDeadlineExceeded,
                     "Retry policy exhausted before first attempt was made.");

  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();

  // `operation` represents the RPC we will make. In the happy case it is just
  // calls to `upload()`, but on a transient error we switch to calling
  // `ResetSession()` until there is a successful result.
  auto* operation = &upload;
  auto const* name = "upload";

  // We need a long-lived object to bind the absl::FunctionRef to.
  auto call_reset = [this](ConstBufferSequence const&) {
    return session_->ResetSession();
  };
  auto reset = UploadChunkFunction(call_reset);

  while (!retry_policy->IsExhausted()) {
    auto const pre_operation_committed_size = committed_size_;
    auto result = (*operation)(buffers);
    AppendDebug(name, static_cast<std::uint64_t>(result.status().code()));
    if (!result) {
      // On a failure we preserve the error, query if retry policy, backoff, and
      // switch to calling ResetSession().
      last_status = std::move(result).status();
      if (!retry_policy->OnFailure(last_status)) {
        return ReturnError(std::move(last_status), *retry_policy, __func__);
      }

      auto delay = backoff_policy->OnCompletion();
      std::this_thread::sleep_for(delay);
      operation = &reset;
      name = "reset";
      continue;
    }

    // While normally a `UploadFinalChunk()` call completes an upload, sometimes
    // the upload can complete in a regular `UploadChunk()` or a
    // `ResetSession()` call. For example, the server can detect a completed
    // upload "early" if the application includes the X-Upload-Content-Length`
    // header.
    if (result->upload_state == ResumableUploadResponse::kDone) return result;

    // This indicates that the response was missing a `Range:` header, or that
    // the range header was in the wrong format. Either way, treat that as a
    // (transient) failure and query the current status to find out what to do
    // next.
    if (!result->committed_size.has_value()) {
      if (operation != &reset) {
        AppendDebug("missing-range-header-on-upload", 0);
        operation = &reset;
        continue;
      }
      // When a reset returns a response without a committed size we can safely
      // treat that as 0.
      result->committed_size = 0;
    }

    // With a successful operation, we can continue (or go back to) uploading.
    operation = &upload;
    name = "upload";

    // This should not happen, it indicates an invalid sequence of responses
    // from the server.
    if (*result->committed_size < pre_operation_committed_size) {
      AppendDebug("loop", *result->committed_size);
      return HandleUncommitError(caller, *result);
    }

    auto const size = TotalBytes(buffers);

    auto const written = *result->committed_size - pre_operation_committed_size;
    committed_size_ = *result->committed_size;

    // On a full write we can return immediately.
    if (written == size) return result;

    // On a partial write we need to write again, but we can skip the bytes that
    // are already committed.
    PopFrontBytes(buffers, static_cast<std::size_t>(written));
  }
  std::ostringstream os;
  os << "Retry policy exhausted in " << caller << ": " << last_status.message();
  return Status(last_status.code(), std::move(os).str());
}

StatusOr<ResumableUploadResponse> RetryResumableUploadSession::ResetSession() {
  Status last_status(StatusCode::kDeadlineExceeded,
                     "Retry policy exhausted before first attempt was made.");
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  while (!retry_policy->IsExhausted()) {
    auto result = session_->ResetSession();
    if (result.ok()) {
      committed_size_ = result->committed_size.value_or(0);
      return result;
    }
    last_status = std::move(result).status();
    if (!retry_policy->OnFailure(last_status)) {
      return ReturnError(std::move(last_status), *retry_policy, __func__);
    }
    auto delay = backoff_policy->OnCompletion();
    std::this_thread::sleep_for(delay);
  }
  std::ostringstream os;
  os << "Retry policy exhausted in " << __func__ << ": "
     << last_status.message();
  return Status(last_status.code(), std::move(os).str());
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

void RetryResumableUploadSession::AppendDebug(char const* action,
                                              std::uint64_t value) {
  std::lock_guard<std::mutex> lk(mu_);
  while (debug_.size() >= 16) debug_.pop_front();
  debug_.push_back(DebugEntry{action, value, std::this_thread::get_id()});
}

Status RetryResumableUploadSession::HandleUncommitError(
    char const* caller, ResumableUploadResponse const& result) {
  std::stringstream os;
  os << caller << ": server previously confirmed " << committed_size_
     << " bytes as committed, but the current response only reports "
     << result.committed_size.value_or(0) << " bytes as committed."
     << " This is most likely a bug in the GCS client library, possibly"
     << " related to parsing the server response."
     << " Please report it at"
     << " https://github.com/googleapis/google-cloud-cpp/issues/new"
     << "    Include as much information as you can including this message";
  os << ", last_response.value=" << result;
  os << ", session_id=" << session_id();
  os << ", debug=[";
  {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto const& e : debug_) {
      os << " {" << e.action << ": " << e.value << " / " << e.tid_ << "}";
    }
  }
  os << "]";
  return Status(StatusCode::kInternal, os.str());
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
