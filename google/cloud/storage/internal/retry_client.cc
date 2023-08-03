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
#include "google/cloud/storage/internal/retry_object_read_source.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/internal/rest_retry_loop.h"
#include "absl/strings/match.h"
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
using ::google::cloud::rest_internal::RestRetryLoop;

// Returns an error if the response contains an unexpected (or invalid)
// committed size.
Status ValidateCommittedSize(UploadChunkRequest const& request,
                             QueryResumableUploadResponse const& response,
                             std::uint64_t expected_committed_size) {
  // This should not happen, it indicates an invalid sequence of responses
  // from the server.
  if (*response.committed_size < request.offset()) {
    std::stringstream os;
    os << __func__ << ": server previously confirmed " << request.offset()
       << " bytes as committed, but the current response only reports "
       << response.committed_size.value_or(0) << " bytes as committed."
       << " This is most likely a bug in the GCS client library, possibly"
       << " related to parsing the server response."
       << " If you believe this is a bug in the client library, please contact"
       << " support (https://cloud.google.com/support/), or report the bug"
       << " (https://github.com/googleapis/google-cloud-cpp/issues/new)."
       << " Please include as much information as you can including this"
       << " message and the following details:";
    os << " session_id=" << request.upload_session_url();
    os << ", result=" << response;
    os << ", request=" << request;
    return Status(StatusCode::kInternal, os.str());
  }
  if (*response.committed_size > expected_committed_size) {
    std::stringstream os;
    os << __func__ << ": the server indicates that "
       << response.committed_size.value_or(0) << " bytes are committed "
       << " but given the current request no more than "
       << expected_committed_size << " are expected be."
       << " Most likely your application resumed an upload, and the client"
       << " library queried the service to find the current persisted bytes."
       << " In some cases, the service is still writing data in the background"
       << " and conservatively reports fewer bytes as persisted."
       << " In this case, the next upload may report a much higher number of"
       << " bytes persisted than expected. It is not possible for the client"
       << " library to recover from this situation. The application needs to"
       << " resume the upload."
       << " This could also be caused by multiple instances of a distributed"
       << " application trying to use the same resumable upload, this is a bug"
       << " in the application."
       << " If you believe this is a bug in the client library, please contact"
       << " support (https://cloud.google.com/support/), or report the bug"
       << " (https://github.com/googleapis/google-cloud-cpp/issues/new)."
       << " Please include as much information as you can including this"
       << " message and the following details:";
    os << " session_id=" << request.upload_session_url();
    os << ", result=" << response;
    os << ", request=" << request;
    return Status(StatusCode::kInternal, os.str());
  }
  return {};
}

// For resumable uploads over gRPC we need to treat some non-retryable errors
// as retryable.
bool UploadChunkOnFailure(RetryPolicy& retry_policy, Status const& status) {
  // TODO(#9273) - use ErrorInfo when it becomes available
  if (status.code() == StatusCode::kAborted &&
      absl::StartsWith(status.message(), "Concurrent requests received.")) {
    return retry_policy.OnFailure(Status(
        StatusCode::kUnavailable, "TODO(#9273) - workaround service problems"));
  }
  return retry_policy.OnFailure(status);
}

Status RetryError(Status const& status, RetryPolicy const& retry_policy,
                  char const* function_name) {
  auto const* msg =
      retry_policy.IsExhausted() ? "Retry policy exhausted" : "Permanent error";
  return google::cloud::internal::RetryLoopError(msg, function_name, status);
}

Status MissingCommittedSize(int error_count, int upload_count, int reset_count,
                            Status last_status) {
  if (error_count > 0) return last_status;
  std::ostringstream os;
  os << "All requests (" << upload_count << ") have succeeded, but they lacked"
     << " a committed_size value. This requires querying the write status."
     << " The client library performed " << reset_count << " such queries.";
  return Status{StatusCode::kDeadlineExceeded, std::move(os).str()};
}

Status PartialWriteStatus(int error_count, int upload_count,
                          std::int64_t committed_size,
                          std::int64_t expected_committed_size,
                          Status last_status) {
  if (error_count > 0) return last_status;
  std::ostringstream os;
  os << "All requests (" << upload_count << ") have succeeded, but they have"
     << " not completed the full write. The expected committed size is "
     << expected_committed_size << " and the current committed size is "
     << committed_size;
  return Status{StatusCode::kDeadlineExceeded, std::move(os).str()};
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
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->ListBuckets(request);
      },
      request, __func__);
}

StatusOr<BucketMetadata> RetryClient::CreateBucket(
    CreateBucketRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->CreateBucket(request);
      },
      request, __func__);
}

StatusOr<BucketMetadata> RetryClient::GetBucketMetadata(
    GetBucketMetadataRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->GetBucketMetadata(request);
      },
      request, __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteBucket(
    DeleteBucketRequest const& request) {
  auto idempotency = current_idempotency_policy().IsIdempotent(request)
                         ? Idempotency::kIdempotent
                         : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->DeleteBucket(request);
      },
      request, __func__);
}

StatusOr<BucketMetadata> RetryClient::UpdateBucket(
    UpdateBucketRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->UpdateBucket(request);
      },
      request, __func__);
}

StatusOr<BucketMetadata> RetryClient::PatchBucket(
    PatchBucketRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->PatchBucket(request);
      },
      request, __func__);
}

StatusOr<NativeIamPolicy> RetryClient::GetNativeBucketIamPolicy(
    GetBucketIamPolicyRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->GetNativeBucketIamPolicy(request);
      },
      request, __func__);
}

StatusOr<NativeIamPolicy> RetryClient::SetNativeBucketIamPolicy(
    SetNativeBucketIamPolicyRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->SetNativeBucketIamPolicy(request);
      },
      request, __func__);
}

StatusOr<TestBucketIamPermissionsResponse>
RetryClient::TestBucketIamPermissions(
    TestBucketIamPermissionsRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->TestBucketIamPermissions(request);
      },
      request, __func__);
}

StatusOr<BucketMetadata> RetryClient::LockBucketRetentionPolicy(
    LockBucketRetentionPolicyRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->LockBucketRetentionPolicy(request);
      },
      request, __func__);
}

StatusOr<ObjectMetadata> RetryClient::InsertObjectMedia(
    InsertObjectMediaRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->InsertObjectMedia(request);
      },
      request, __func__);
}

StatusOr<ObjectMetadata> RetryClient::CopyObject(
    CopyObjectRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->CopyObject(request);
      },
      request, __func__);
}

StatusOr<ObjectMetadata> RetryClient::GetObjectMetadata(
    GetObjectMetadataRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->GetObjectMetadata(request);
      },
      request, __func__);
}

StatusOr<std::unique_ptr<ObjectReadSource>> RetryClient::ReadObjectNotWrapped(
    ReadObjectRangeRequest const& request, RetryPolicy& retry_policy,
    BackoffPolicy& backoff_policy) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      retry_policy, backoff_policy, idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->ReadObject(request);
      },
      request, __func__);
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
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->ListObjects(request);
      },
      request, __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteObject(
    DeleteObjectRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->DeleteObject(request);
      },
      request, __func__);
}

StatusOr<ObjectMetadata> RetryClient::UpdateObject(
    UpdateObjectRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->UpdateObject(request);
      },
      request, __func__);
}

StatusOr<ObjectMetadata> RetryClient::PatchObject(
    PatchObjectRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->PatchObject(request);
      },
      request, __func__);
}

StatusOr<ObjectMetadata> RetryClient::ComposeObject(
    ComposeObjectRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->ComposeObject(request);
      },
      request, __func__);
}

StatusOr<RewriteObjectResponse> RetryClient::RewriteObject(
    RewriteObjectRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->RewriteObject(request);
      },
      request, __func__);
}

StatusOr<CreateResumableUploadResponse> RetryClient::CreateResumableUpload(
    ResumableUploadRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->CreateResumableUpload(request);
      },
      request, __func__);
}

StatusOr<QueryResumableUploadResponse> RetryClient::QueryResumableUpload(
    QueryResumableUploadRequest const& request) {
  auto const idempotency = Idempotency::kIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->QueryResumableUpload(request);
      },
      request, __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteResumableUpload(
    DeleteResumableUploadRequest const& request) {
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(),
      Idempotency::kIdempotent,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->DeleteResumableUpload(request);
      },
      request, __func__);
}

// Implements the retry loop for a resumable upload session.
//
// A description of resumable uploads can be found at:
//     https://cloud.google.com/storage/docs/performing-resumable-uploads
//
// A description of the gRPC analog can be found in the proto file. Pay
// particular attention to the documentation for `WriteObject()`,
// `WriteObjectRequest`, `StartResumableWrite()` and `QueryResumableWrite()`:
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
  auto sleeper = google::cloud::internal::MakeTracedSleeper(
      [](std::chrono::milliseconds d) { std::this_thread::sleep_for(d); });
  auto last_status =
      Status(StatusCode::kDeadlineExceeded,
             "Retry policy exhausted before first attempt was made.");

  auto retry_policy = current_retry_policy();
  auto backoff_policy = current_backoff_policy();

  // `operation` represents the RPC we will make. In the happy case it is just
  // calls to `upload()`, but on a transient error we switch to calling
  // `ResetSession()` until there is a successful result.
  using Action =
      std::function<StatusOr<QueryResumableUploadResponse>(std::uint64_t)>;

  int upload_count = 0;
  auto upload =
      Action([&upload_count, &request, this](std::uint64_t committed_size) {
        ++upload_count;
        return client_->UploadChunk(request.RemainingChunk(committed_size));
      });
  int reset_count = 0;
  auto reset = Action([&reset_count, &request, this](std::uint64_t) {
    QueryResumableUploadRequest query(request.upload_session_url());
    query.set_multiple_options(request.GetOption<QuotaUser>(),
                               request.GetOption<UserIp>());
    ++reset_count;
    return this->QueryResumableUpload(query);
  });

  auto* operation = &upload;
  auto committed_size = request.offset();
  auto const expected_committed_size =
      request.offset() + request.payload_size();

  int error_count = 0;

  while (!retry_policy->IsExhausted()) {
    auto result = (*operation)(committed_size);
    if (!result) {
      ++error_count;
      // On a failure we preserve the error, then query if retry policy allows
      // retrying.  If so, we backoff, and switch to calling
      // QueryResumableUpload().
      last_status = std::move(result).status();
      if (!UploadChunkOnFailure(*retry_policy, last_status)) {
        return RetryError(std::move(last_status), *retry_policy, __func__);
      }

      auto delay = backoff_policy->OnCompletion();
      sleeper(delay);
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
      last_status = MissingCommittedSize(error_count, upload_count, reset_count,
                                         std::move(last_status));
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

    auto validate =
        ValidateCommittedSize(request, *result, expected_committed_size);
    if (!validate.ok()) return validate;

    committed_size = *result->committed_size;

    if (committed_size != expected_committed_size || request.last_chunk()) {
      // If we still have to send data, restart the loop. On the last chunk,
      // even if the service reports all the data as received, we need to keep
      // "finalizing" the object until the object metadata is returned. Note
      // that if we had the object metadata we would have already exited this
      // function.
      last_status =
          PartialWriteStatus(error_count, upload_count, committed_size,
                             expected_committed_size, std::move(last_status));
      continue;
    }

    // On a full write we can return immediately.
    return result;
  }
  return RetryError(last_status, *retry_policy, __func__);
}

StatusOr<ListBucketAclResponse> RetryClient::ListBucketAcl(
    ListBucketAclRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->ListBucketAcl(request);
      },
      request, __func__);
}

StatusOr<BucketAccessControl> RetryClient::GetBucketAcl(
    GetBucketAclRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->GetBucketAcl(request);
      },
      request, __func__);
}

StatusOr<BucketAccessControl> RetryClient::CreateBucketAcl(
    CreateBucketAclRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->CreateBucketAcl(request);
      },
      request, __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteBucketAcl(
    DeleteBucketAclRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->DeleteBucketAcl(request);
      },
      request, __func__);
}

StatusOr<ListObjectAclResponse> RetryClient::ListObjectAcl(
    ListObjectAclRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->ListObjectAcl(request);
      },
      request, __func__);
}

StatusOr<BucketAccessControl> RetryClient::UpdateBucketAcl(
    UpdateBucketAclRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->UpdateBucketAcl(request);
      },
      request, __func__);
}

StatusOr<BucketAccessControl> RetryClient::PatchBucketAcl(
    PatchBucketAclRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->PatchBucketAcl(request);
      },
      request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::CreateObjectAcl(
    CreateObjectAclRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->CreateObjectAcl(request);
      },
      request, __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteObjectAcl(
    DeleteObjectAclRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->DeleteObjectAcl(request);
      },
      request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::GetObjectAcl(
    GetObjectAclRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->GetObjectAcl(request);
      },
      request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::UpdateObjectAcl(
    UpdateObjectAclRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->UpdateObjectAcl(request);
      },
      request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::PatchObjectAcl(
    PatchObjectAclRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->PatchObjectAcl(request);
      },
      request, __func__);
}

StatusOr<ListDefaultObjectAclResponse> RetryClient::ListDefaultObjectAcl(
    ListDefaultObjectAclRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->ListDefaultObjectAcl(request);
      },
      request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::CreateDefaultObjectAcl(
    CreateDefaultObjectAclRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->CreateDefaultObjectAcl(request);
      },
      request, __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteDefaultObjectAcl(
    DeleteDefaultObjectAclRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->DeleteDefaultObjectAcl(request);
      },
      request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::GetDefaultObjectAcl(
    GetDefaultObjectAclRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->GetDefaultObjectAcl(request);
      },
      request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::UpdateDefaultObjectAcl(
    UpdateDefaultObjectAclRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->UpdateDefaultObjectAcl(request);
      },
      request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::PatchDefaultObjectAcl(
    PatchDefaultObjectAclRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->PatchDefaultObjectAcl(request);
      },
      request, __func__);
}

StatusOr<ServiceAccount> RetryClient::GetServiceAccount(
    GetProjectServiceAccountRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->GetServiceAccount(request);
      },
      request, __func__);
}

StatusOr<ListHmacKeysResponse> RetryClient::ListHmacKeys(
    ListHmacKeysRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->ListHmacKeys(request);
      },
      request, __func__);
}

StatusOr<CreateHmacKeyResponse> RetryClient::CreateHmacKey(
    CreateHmacKeyRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->CreateHmacKey(request);
      },
      request, __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteHmacKey(
    DeleteHmacKeyRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->DeleteHmacKey(request);
      },
      request, __func__);
}

StatusOr<HmacKeyMetadata> RetryClient::GetHmacKey(
    GetHmacKeyRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->GetHmacKey(request);
      },
      request, __func__);
}

StatusOr<HmacKeyMetadata> RetryClient::UpdateHmacKey(
    UpdateHmacKeyRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->UpdateHmacKey(request);
      },
      request, __func__);
}

StatusOr<SignBlobResponse> RetryClient::SignBlob(
    SignBlobRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->SignBlob(request);
      },
      request, __func__);
}

StatusOr<ListNotificationsResponse> RetryClient::ListNotifications(
    ListNotificationsRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->ListNotifications(request);
      },
      request, __func__);
}

StatusOr<NotificationMetadata> RetryClient::CreateNotification(
    CreateNotificationRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->CreateNotification(request);
      },
      request, __func__);
}

StatusOr<NotificationMetadata> RetryClient::GetNotification(
    GetNotificationRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->GetNotification(request);
      },
      request, __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteNotification(
    DeleteNotificationRequest const& request) {
  auto const idempotency = current_idempotency_policy().IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return RestRetryLoop(
      current_retry_policy(), current_backoff_policy(), idempotency,
      [this](rest_internal::RestContext&, auto const& request) {
        return client_->DeleteNotification(request);
      },
      request, __func__);
}

std::vector<std::string> RetryClient::InspectStackStructure() const {
  auto stack = client_->InspectStackStructure();
  stack.emplace_back("RetryClient");
  return stack;
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
