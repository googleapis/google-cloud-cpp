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

#include "google/cloud/storage/internal/retry_client.h"
#include "google/cloud/storage/internal/raw_client_wrapper_utils.h"
#include "google/cloud/storage/internal/retry_object_read_source.h"
#include "google/cloud/internal/retry_policy.h"
#include "absl/memory/memory.h"
#include <sstream>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::Idempotency;
using ::google::cloud::internal::MergeOptions;
using ::google::cloud::storage::internal::raw_client_wrapper_utils::Signature;

/**
 * Calls a client operation with retries borrowing the RPC policies.
 *
 * @tparam MemberFunction the signature of the member function.
 * @param client the storage::Client object to make the call through.
 * @param retry_policy the policy controlling what failures are retryable, and
 *     for how long we can retry
 * @param backoff_policy the policy controlling how long to wait before
 *     retrying.
 * @param function the pointer to the member function to call.
 * @param request an initialized request parameter for the call.
 * @param error_message include this message in any exception or error log.
 * @return the result from making the call;
 * @throw std::exception with a description of the last error.
 */
template <typename MemberFunction>
typename Signature<MemberFunction>::ReturnType MakeCall(
    RetryPolicy& retry_policy, BackoffPolicy& backoff_policy,
    Idempotency idempotency, RawClient& client, MemberFunction function,
    typename Signature<MemberFunction>::RequestType const& request,
    char const* error_message) {
  Status last_status(StatusCode::kDeadlineExceeded,
                     "Retry policy exhausted before first attempt was made.");
  auto error = [&last_status](std::string const& msg) {
    return Status(last_status.code(), msg);
  };

  while (!retry_policy.IsExhausted()) {
    auto result = (client.*function)(request);
    if (result.ok()) {
      return result;
    }
    last_status = std::move(result).status();
    if (idempotency == Idempotency::kNonIdempotent) {
      std::ostringstream os;
      os << "Error in non-idempotent operation " << error_message << ": "
         << last_status.message();
      return error(std::move(os).str());
    }
    if (!retry_policy.OnFailure(last_status)) {
      if (internal::StatusTraits::IsPermanentFailure(last_status)) {
        // The last error cannot be retried, but it is not because the retry
        // policy is exhausted, we call these "permanent errors", and they
        // get a special message.
        std::ostringstream os;
        os << "Permanent error in " << error_message << ": "
           << last_status.message();
        return error(std::move(os).str());
      }
      // Exit the loop immediately instead of sleeping before trying again.
      break;
    }
    auto delay = backoff_policy.OnCompletion();
    std::this_thread::sleep_for(delay);
  }
  std::ostringstream os;
  os << "Retry policy exhausted in " << error_message << ": "
     << last_status.message();
  return error(std::move(os).str());
}

}  // namespace

std::shared_ptr<RetryClient> RetryClient::Create(
    std::shared_ptr<RawClient> client, Options options) {
  // Cannot use `std::make_shared<>` because the constructor is private.
  return std::shared_ptr<RetryClient>(
      new RetryClient(std::move(client), std::move(options)));
}

RetryClient::RetryClient(std::shared_ptr<RawClient> client, Options options)
    : client_(std::move(client)),
      options_(MergeOptions(std::move(options), client_->options())) {}

ClientOptions const& RetryClient::client_options() const {
  return client_->client_options();
}

Options RetryClient::options() const { return options_; }

StatusOr<ListBucketsResponse> RetryClient::ListBuckets(
    ListBucketsRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::ListBuckets, request, __func__);
}

StatusOr<BucketMetadata> RetryClient::CreateBucket(
    CreateBucketRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::CreateBucket, request, __func__);
}

StatusOr<BucketMetadata> RetryClient::GetBucketMetadata(
    GetBucketMetadataRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::GetBucketMetadata, request, __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteBucket(
    DeleteBucketRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto idempotency = current_idempotency_policy().IsIdempotent(request)
                         ? Idempotency::kIdempotent
                         : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::DeleteBucket, request, __func__);
}

StatusOr<BucketMetadata> RetryClient::UpdateBucket(
    UpdateBucketRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::UpdateBucket, request, __func__);
}

StatusOr<BucketMetadata> RetryClient::PatchBucket(
    PatchBucketRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::PatchBucket, request, __func__);
}

StatusOr<NativeIamPolicy> RetryClient::GetNativeBucketIamPolicy(
    GetBucketIamPolicyRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::GetNativeBucketIamPolicy, request, __func__);
}

StatusOr<NativeIamPolicy> RetryClient::SetNativeBucketIamPolicy(
    SetNativeBucketIamPolicyRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::SetNativeBucketIamPolicy, request, __func__);
}

StatusOr<TestBucketIamPermissionsResponse>
RetryClient::TestBucketIamPermissions(
    TestBucketIamPermissionsRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::TestBucketIamPermissions, request, __func__);
}

StatusOr<BucketMetadata> RetryClient::LockBucketRetentionPolicy(
    LockBucketRetentionPolicyRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::LockBucketRetentionPolicy, request, __func__);
}

StatusOr<ObjectMetadata> RetryClient::InsertObjectMedia(
    InsertObjectMediaRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::InsertObjectMedia, request, __func__);
}

StatusOr<ObjectMetadata> RetryClient::CopyObject(
    CopyObjectRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::CopyObject, request, __func__);
}

StatusOr<ObjectMetadata> RetryClient::GetObjectMetadata(
    GetObjectMetadataRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::GetObjectMetadata, request, __func__);
}

StatusOr<std::unique_ptr<ObjectReadSource>> RetryClient::ReadObjectNotWrapped(
    ReadObjectRangeRequest const& request, RetryPolicy& retry_policy,
    BackoffPolicy& backoff_policy) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(retry_policy, backoff_policy, idempotency, *client_,
                  &RawClient::ReadObject, request, __func__);
}

StatusOr<std::unique_ptr<ObjectReadSource>> RetryClient::ReadObject(
    ReadObjectRangeRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto child = ReadObjectNotWrapped(request, *retry_policy, *backoff_policy);
  if (!child) {
    return child;
  }
  auto self = shared_from_this();
  return std::unique_ptr<ObjectReadSource>(new RetryObjectReadSource(
      self, request, *std::move(child), std::move(retry_policy),
      std::move(backoff_policy)));
}

StatusOr<ListObjectsResponse> RetryClient::ListObjects(
    ListObjectsRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::ListObjects, request, __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteObject(
    DeleteObjectRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::DeleteObject, request, __func__);
}

StatusOr<ObjectMetadata> RetryClient::UpdateObject(
    UpdateObjectRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::UpdateObject, request, __func__);
}

StatusOr<ObjectMetadata> RetryClient::PatchObject(
    PatchObjectRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::PatchObject, request, __func__);
}

StatusOr<ObjectMetadata> RetryClient::ComposeObject(
    ComposeObjectRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::ComposeObject, request, __func__);
}

StatusOr<RewriteObjectResponse> RetryClient::RewriteObject(
    RewriteObjectRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::RewriteObject, request, __func__);
}

StatusOr<CreateResumableUploadResponse> RetryClient::CreateResumableUpload(
    ResumableUploadRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::CreateResumableUpload, request, __func__);
}

StatusOr<QueryResumableUploadResponse> RetryClient::QueryResumableUpload(
    QueryResumableUploadRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = Idempotency::kIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::QueryResumableUpload, request, __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteResumableUpload(
    DeleteResumableUploadRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  return MakeCall(*retry_policy, *backoff_policy, Idempotency::kIdempotent,
                  *client_, &RawClient::DeleteResumableUpload, request,
                  __func__);
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
StatusOr<QueryResumableUploadResponse> RetryClient::UploadChunk(
    UploadChunkRequest const& request) {
  Status last_status(StatusCode::kDeadlineExceeded,
                     "Retry policy exhausted before first attempt was made.");

  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();

  // `operation` represents the RPC we will make. In the happy case it is just
  // calls to `upload()`, but on a transient error we switch to calling
  // `ResetSession()` until there is a successful result.
  using Action =
      std::function<StatusOr<QueryResumableUploadResponse>(std::uint64_t)>;

  auto upload = Action([&request, this](std::uint64_t committed_size) {
    return client_->UploadChunk(request.RemainingChunk(committed_size));
  });
  auto reset = Action([&request, this](std::uint64_t) {
    QueryResumableUploadRequest query(request.upload_session_url());
    query.set_multiple_options(request.GetOption<QuotaUser>(),
                               request.GetOption<UserIp>());
    return this->QueryResumableUpload(query);
  });

  auto return_error = [](Status const& last_status,
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
  };

  auto* operation = &upload;
  auto committed_size = request.offset();
  auto const expected_committed_size =
      request.offset() + request.payload_size();

  while (!retry_policy->IsExhausted()) {
    auto result = (*operation)(committed_size);
    if (!result) {
      // On a failure we preserve the error, then query if retry policy allows
      // retrying.  If so, we backoff, and switch to calling
      // QueryResumableUpload().
      last_status = std::move(result).status();
      // For resumable uploads over gRPC some kAborted errors are retryable
      // error.
      // TODO(#9273) - use ErrorInfo when it becomes available
      auto constexpr kConcurrentMessagePrefix = "Concurrent requests received.";
      auto const is_concurrent_write =
          last_status.code() == StatusCode::kAborted &&
          last_status.message().rfind(kConcurrentMessagePrefix, 0) == 0;
      auto const is_retryable =
          is_concurrent_write
              ? retry_policy->OnFailure(Status(StatusCode::kUnavailable, ""))
              : retry_policy->OnFailure(last_status);
      if (!is_retryable) {
        return return_error(std::move(last_status), *retry_policy, __func__);
      }

      auto delay = backoff_policy->OnCompletion();
      std::this_thread::sleep_for(delay);
      operation = &reset;
      continue;
    }

    // While normally a `UploadFinalChunk()` call completes an upload, sometimes
    // the upload can complete in a regular `UploadChunk()` or a
    // `ResetSession()` call. For example, the server can detect a completed
    // upload "early" if the application includes the X-Upload-Content-Length`
    // header.
    if (result->payload.has_value()) return result;

    // This indicates that the response was missing a `Range:` header, or that
    // the range header was in the wrong format. Either way, treat that as a
    // (transient) failure and query the current status to find out what to do
    // next.
    if (!result->committed_size.has_value()) {
      if (operation != &reset) {
        operation = &reset;
        continue;
      }
      // When a reset returns a response without a committed size we can safely
      // treat that as 0.
      result->committed_size = 0;
    }

    // With a successful operation, we can continue (or go back to) uploading.
    operation = &upload;

    // This should not happen, it indicates an invalid sequence of responses
    // from the server.
    if (*result->committed_size < request.offset()) {
      std::stringstream os;
      os << __func__ << ": server previously confirmed " << request.offset()
         << " bytes as committed, but the current response only reports "
         << result->committed_size.value_or(0) << " bytes as committed."
         << " This is most likely a bug in the GCS client library, possibly"
         << " related to parsing the server response."
         << " Please contact support or report the problem at"
         << " https://github.com/googleapis/google-cloud-cpp/issues/new"
         << " Include as much information as you can including this message";
      os << ", session_id=" << request.upload_session_url();
      os << ", result=" << *result;
      os << ", request=" << request;
      return Status(StatusCode::kInternal, os.str());
    }
    if (*result->committed_size > expected_committed_size) {
      std::stringstream os;
      os << __func__ << ": the server indicates that "
         << result->committed_size.value_or(0) << " bytes are committed "
         << " but given the current request no more than "
         << expected_committed_size << " should be."
         << " This could be caused by multiple applications trying to use the"
         << " same resumable upload, but could be a bug in the library or"
         << " the service. If you believe this is a bug, please contact support"
         << " or report the bug at"
         << " https://github.com/googleapis/google-cloud-cpp/issues/new"
         << " Include as much information as you can including this message";
      os << ", session_id=" << request.upload_session_url();
      os << ", result=" << *result;
      os << ", request=" << request;
      return Status(StatusCode::kInternal, os.str());
    }

    committed_size = *result->committed_size;

    if (committed_size != expected_committed_size || request.last_chunk()) {
      // If we still have to send data, restart the loop. On the last chunk,
      // even if the service reports all the data as received, we need to keep
      // "finalizing" the object until the object metadata is returned. Note
      // that if we had the object metadata we would have already exited this
      // function.
      continue;
    }

    // On a full write we can return immediately.
    return result;
  }
  std::ostringstream os;
  os << "Retry policy exhausted in " << __func__ << ": "
     << last_status.message();
  return Status(last_status.code(), std::move(os).str());
}

StatusOr<ListBucketAclResponse> RetryClient::ListBucketAcl(
    ListBucketAclRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::ListBucketAcl, request, __func__);
}

StatusOr<BucketAccessControl> RetryClient::GetBucketAcl(
    GetBucketAclRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::GetBucketAcl, request, __func__);
}

StatusOr<BucketAccessControl> RetryClient::CreateBucketAcl(
    CreateBucketAclRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::CreateBucketAcl, request, __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteBucketAcl(
    DeleteBucketAclRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::DeleteBucketAcl, request, __func__);
}

StatusOr<ListObjectAclResponse> RetryClient::ListObjectAcl(
    ListObjectAclRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::ListObjectAcl, request, __func__);
}

StatusOr<BucketAccessControl> RetryClient::UpdateBucketAcl(
    UpdateBucketAclRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::UpdateBucketAcl, request, __func__);
}

StatusOr<BucketAccessControl> RetryClient::PatchBucketAcl(
    PatchBucketAclRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::PatchBucketAcl, request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::CreateObjectAcl(
    CreateObjectAclRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::CreateObjectAcl, request, __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteObjectAcl(
    DeleteObjectAclRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::DeleteObjectAcl, request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::GetObjectAcl(
    GetObjectAclRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::GetObjectAcl, request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::UpdateObjectAcl(
    UpdateObjectAclRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::UpdateObjectAcl, request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::PatchObjectAcl(
    PatchObjectAclRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::PatchObjectAcl, request, __func__);
}

StatusOr<ListDefaultObjectAclResponse> RetryClient::ListDefaultObjectAcl(
    ListDefaultObjectAclRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::ListDefaultObjectAcl, request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::CreateDefaultObjectAcl(
    CreateDefaultObjectAclRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::CreateDefaultObjectAcl, request, __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteDefaultObjectAcl(
    DeleteDefaultObjectAclRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::DeleteDefaultObjectAcl, request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::GetDefaultObjectAcl(
    GetDefaultObjectAclRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::GetDefaultObjectAcl, request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::UpdateDefaultObjectAcl(
    UpdateDefaultObjectAclRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::UpdateDefaultObjectAcl, request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::PatchDefaultObjectAcl(
    PatchDefaultObjectAclRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::PatchDefaultObjectAcl, request, __func__);
}

StatusOr<ServiceAccount> RetryClient::GetServiceAccount(
    GetProjectServiceAccountRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::GetServiceAccount, request, __func__);
}

StatusOr<ListHmacKeysResponse> RetryClient::ListHmacKeys(
    ListHmacKeysRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::ListHmacKeys, request, __func__);
}

StatusOr<CreateHmacKeyResponse> RetryClient::CreateHmacKey(
    CreateHmacKeyRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::CreateHmacKey, request, __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteHmacKey(
    DeleteHmacKeyRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::DeleteHmacKey, request, __func__);
}

StatusOr<HmacKeyMetadata> RetryClient::GetHmacKey(
    GetHmacKeyRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::GetHmacKey, request, __func__);
}

StatusOr<HmacKeyMetadata> RetryClient::UpdateHmacKey(
    UpdateHmacKeyRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::UpdateHmacKey, request, __func__);
}

StatusOr<SignBlobResponse> RetryClient::SignBlob(
    SignBlobRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::SignBlob, request, __func__);
}

StatusOr<ListNotificationsResponse> RetryClient::ListNotifications(
    ListNotificationsRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::ListNotifications, request, __func__);
}

StatusOr<NotificationMetadata> RetryClient::CreateNotification(
    CreateNotificationRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::CreateNotification, request, __func__);
}

StatusOr<NotificationMetadata> RetryClient::GetNotification(
    GetNotificationRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::GetNotification, request, __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteNotification(
    DeleteNotificationRequest const& request) {
  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::DeleteNotification, request, __func__);
}

std::unique_ptr<RetryPolicy> RetryClient::current_retry_policy() {
  return google::cloud::internal::CurrentOptions()
      .get<RetryPolicyOption>()
      ->clone();
}

std::unique_ptr<BackoffPolicy> RetryClient::current_backoff_policy() {
  return google::cloud::internal::CurrentOptions()
      .get<BackoffPolicyOption>()
      ->clone();
}

IdempotencyPolicy& RetryClient::current_idempotency_policy() {
  return *google::cloud::internal::CurrentOptions()
              .get<IdempotencyPolicyOption>();
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
