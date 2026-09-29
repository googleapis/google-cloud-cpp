// Copyright 2026 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RETRY_LOGGING_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RETRY_LOGGING_H

#include "google/cloud/retry_policy.h"
#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include <memory>
#include <string>
#include <string_view>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Diagnostics for silently retried transient errors.
//
// When GCS returns a transient error (HTTP 429, 503, 500, or a stalled
// connection) the client library backs off and retries, or resumes an
// interrupted download. Historically this happened with no output at all, so an
// application would simply appear to stall: there was no way to attribute a
// latency spike to service-side throttling.
//
// The functions below emit one `WARNING` per retried attempt. They are
// deliberately *not* gated behind `LoggingComponentsOption`: a log that must be
// enabled ahead of time is of no use during the first occurrence of an
// incident. This is inexpensive by default because the default log backend
// discards everything below `FATAL` (see `DefaultLogBackend()` in
// `google/cloud/log.cc`). Applications that want the diagnostics either set
// `GOOGLE_CLOUD_CPP_ENABLE_CLOG`, or register their own backend via
// `LogSink::AddBackend()`.

/**
 * Emits a single `WARNING` describing a transient failure that is being
 * retried or resumed.
 *
 * The record is always exactly one line: the status message comes from the
 * service and may contain newlines, which would otherwise break `grep` and
 * would let a misbehaving service forge log lines.
 *
 * @param where the operation being retried, normally `__func__`.
 * @param resource the affected resource, normally formatted as
 *     `"bucket/object"`. Operations that have no such name (a resumable upload,
 *     for example) pass whatever identifier they do have. Pass an empty view
 *     when the operation is not tied to a single resource.
 * @param status the failure that triggered the retry.
 * @param attempt the 1-based number of the attempt that just failed.
 */
void LogTransientRetry(char const* where, std::string_view resource,
                       Status const& status, int attempt);

/**
 * A `RetryPolicy` decorator that logs every attempt the policy decides to
 * retry.
 *
 * Use this to instrument a retry loop that only has the policy to work with,
 * such as `RestRetryLoop()` or `AsyncRetryLoop()`. Wrap the real policy, pass
 * the decorator to the loop, and each retried attempt produces one record.
 *
 * The decision comes from the wrapped policy rather than from a separate
 * classification of the status, which matters for two reasons:
 *
 * - A record is emitted only when another attempt actually follows. The final
 *   attempt of an exhausted policy, and a permanent error, are both silent;
 *   those failures are reported to the caller through the returned `Status`.
 * - There is no second copy of "which errors are retryable" to drift out of
 *   step with the policy in force. A caller that overrides the retry policy
 *   automatically gets logging that matches it. This also keeps the sync and
 *   async paths correct without a shared table: they use different definitions
 *   of "transient" (only the async traits treat `kAborted` as retryable), and
 *   each one is the definition its own policy applies.
 *
 * `OnFailure()` is not reached for a non-idempotent request under
 * `RestRetryLoop()`, which returns before consulting the policy, so those are
 * silent. `AsyncRetryLoop()` calls `OnFailure()` before it checks idempotency,
 * so a non-idempotent async request would still be reported; no async call site
 * passes `kNonIdempotent` today.
 *
 * Two constructors are provided because the two loops own the policy
 * differently: `RestRetryLoop()` borrows a `RetryPolicy&` from the caller,
 * while `AsyncRetryLoop()` takes a `std::unique_ptr` and outlives the caller's
 * stack frame.
 */
class LoggingRetryPolicy : public google::cloud::RetryPolicy {
 public:
  /// Borrows @p impl, which must outlive this object.
  LoggingRetryPolicy(google::cloud::RetryPolicy& impl, char const* where,
                     std::string resource);

  /// Takes ownership of @p impl, which must not be null.
  LoggingRetryPolicy(std::unique_ptr<google::cloud::RetryPolicy> impl,
                     char const* where, std::string resource);

  bool OnFailure(Status const& status) override;
  bool IsExhausted() const override;
  bool IsPermanentFailure(Status const& status) const override;

  /// The number of attempts reported so far. Exposed for tests.
  int attempt_count() const { return attempt_; }

 private:
  // Empty when the policy is borrowed. Declared before `impl_` so that the
  // owning constructor can bind the reference to it.
  std::unique_ptr<google::cloud::RetryPolicy> owned_;
  google::cloud::RetryPolicy& impl_;
  char const* where_;
  std::string resource_;
  int attempt_ = 0;
};

/**
 * Formats @p bucket_name and @p object_name for the `resource` argument above.
 *
 * Returns whichever name is present when the other is empty, and an empty
 * string when both are.
 */
std::string RetryLogResource(std::string_view bucket_name,
                             std::string_view object_name);

/**
 * Formats a resumable upload session identifier for the `resource` argument
 * above, with the session token redacted.
 *
 * A resumable upload session token is a bearer capability: anyone who has it
 * can append to, or finalize, the upload without any other credential. These
 * diagnostics are always on and usually end up in a log aggregator that more
 * people can read than the bucket itself, so only a short prefix of the token
 * is kept. That is enough to correlate the log lines belonging to one upload,
 * and not enough to hijack it.
 *
 * Both transports are handled, because `UploadChunkRequest` carries whichever
 * form the transport in use produced:
 *
 * - REST passes a URL whose `upload_id=` query parameter holds the token. The
 *   surrounding URL names the bucket and is kept.
 * - gRPC may pass the resource-name form,
 *   `projects/<project>/buckets/<bucket>/<token>`, where the token may itself
 *   contain `/`. The bucket is kept and everything after it is redacted.
 *
 * Anything else is redacted in its entirety. That default is deliberate: gRPC
 * also hands back wholly opaque tokens with no structure to recognise, so
 * treating an unfamiliar value as safe would leak the whole capability the
 * first time the service changed the format.
 */
std::string RetryLogUploadResource(std::string_view upload_session_url);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RETRY_LOGGING_H
