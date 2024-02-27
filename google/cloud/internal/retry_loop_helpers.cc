// Copyright 2020 Google LLC
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

#include "google/cloud/internal/retry_loop_helpers.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/make_status.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

void AddErrorMetadata(ErrorInfo& ei, Status const& status, char const* location,
                      char const* reason) {
  AddMetadata(ei, "gcloud-cpp.retry.original-message", status.message());
  AddMetadata(ei, "gcloud-cpp.retry.function", location);
  AddMetadata(ei, "gcloud-cpp.retry.reason", reason);
}

void AddOnEntry(ErrorInfo& ei, char const* value) {
  AddMetadata(ei, "gcloud-cpp.retry.on-entry", value);
}

ErrorInfoBuilder AddErrorMetadata(ErrorInfoBuilder b, Status const& status,
                                  char const* location, char const* reason) {
  return std::move(b)
      .WithMetadata("gcloud-cpp.retry.original-message", status.message())
      .WithMetadata("gcloud-cpp.retry.function", location)
      .WithMetadata("gcloud-cpp.retry.reason", reason);
}

}  // namespace

Status RetryLoopNonIdempotentError(Status status, char const* location) {
  if (status.ok()) return status;
  auto ei = status.error_info();
  AddErrorMetadata(ei, status, location, "non-idempotent");
  auto message =
      absl::StrCat("Error in non-idempotent operation: ", status.message());
  return Status(status.code(), std::move(message), std::move(ei));
}

Status RetryLoopError(Status const& status, char const* location,
                      bool exhausted) {
  if (exhausted) return RetryLoopPolicyExhaustedError(status, location);
  // If the error cannot be retried, and the retry policy is not exhausted we
  // call the error a "permanent error".
  return RetryLoopPermanentError(status, location);
}

Status RetryLoopPermanentError(Status const& status, char const* location) {
  if (status.ok()) {
    return UnknownError(
        absl::StrCat("Retry policy treats kOk as permanent error"),
        AddErrorMetadata(GCP_ERROR_INFO(), status, location,
                         "permanent-error"));
  }
  auto ei = status.error_info();
  AddErrorMetadata(ei, status, location, "permanent-error");
  auto message = absl::StrCat("Permanent error, with a last message of ",
                              status.message());
  return Status(status.code(), std::move(message), std::move(ei));
}

Status RetryLoopPolicyExhaustedError(Status const& status,
                                     char const* location) {
  if (status.ok()) {
    // This indicates the retry loop never made a request.
    return DeadlineExceededError(
        absl::StrCat("Retry policy exhausted before first request attempt"),
        AddErrorMetadata(GCP_ERROR_INFO(), status, location,
                         "retry-policy-exhausted")
            .WithMetadata("gcloud-cpp.retry.on-entry", "true"));
  }
  auto ei = status.error_info();
  AddErrorMetadata(ei, status, location, "retry-policy-exhausted");
  AddOnEntry(ei, "false");
  auto message = absl::StrCat("Retry policy exhausted, with a last message of ",
                              status.message());
  return Status(status.code(), std::move(message), std::move(ei));
}

Status RetryLoopCancelled(Status const& status, char const* location) {
  if (status.ok()) {
    // This indicates the retry loop never made a request.
    return CancelledError(
        absl::StrCat("Retry policy cancelled"),
        AddErrorMetadata(GCP_ERROR_INFO(), status, location, "cancelled"));
  }
  auto ei = status.error_info();
  AddErrorMetadata(ei, status, location, "cancelled");
  auto message = absl::StrCat("Retry loop cancelled, with a last message of ",
                              status.message());
  return Status(status.code(), std::move(message), std::move(ei));
}

StatusOr<std::chrono::milliseconds> Backoff(Status const& status,
                                            char const* location,
                                            RetryPolicy& retry,
                                            BackoffPolicy& backoff,
                                            Idempotency idempotency,
                                            bool enable_server_retries) {
  bool should_retry = retry.OnFailure(status);
  if (enable_server_retries) {
    auto ri = internal::GetRetryInfo(status);
    if (ri) {
      if (retry.IsExhausted()) {
        return RetryLoopPolicyExhaustedError(status, location);
      }
      // Ping the backoff policy, but ignore the result. We do the same with the
      // retry policy.
      (void)backoff.OnCompletion();
      return std::chrono::duration_cast<std::chrono::milliseconds>(
          ri->retry_delay());
    }
  }
  if (idempotency == Idempotency::kNonIdempotent) {
    return RetryLoopNonIdempotentError(status, location);
  }
  if (should_retry) return backoff.OnCompletion();
  return RetryLoopError(status, location, retry.IsExhausted());
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
