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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_HEDGED_OPEN_STREAM_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_HEDGED_OPEN_STREAM_H

#include "google/cloud/storage/async/retry_policy.h"
#include "google/cloud/storage/internal/async/open_stream.h"
#include "google/cloud/storage/internal/hedge_limiter.h"
#include "google/cloud/storage/internal/hedged_read_metrics.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "google/storage/v2/storage.pb.h"
#include <chrono>
#include <memory>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

struct HedgedOpenParams {
  std::chrono::milliseconds delay;
  int max_hedges;
};

/**
 * Races a primary `OpenStreamFactory` attempt against up to `params.max_hedges`
 * delayed speculative attempts spaced by `params.delay`.
 *
 * The first attempt to succeed wins; remaining in-flight attempts and timers
 * are cancelled, and any late-completing loser's `OpenStream` is cancelled via
 * `OpenStream::Cancel()`. A permanent non-`kCancelled` error from *any* attempt
 * resolves the race immediately; otherwise if all launched attempts fail, the
 * primary attempt's error is returned. Cancelling the returned future cancels
 * all in-flight attempts and timers. Negative `params.delay` and
 * `params.max_hedges` values are clamped to zero.
 *
 * @p retry_policy classifies permanent failures; if null, no attempt error
 * aborts siblings early.
 * @p limiter gates speculative hedge dispatches across the connection; if null,
 * per-connection rate/concurrency/lifetime limits are not enforced.
 * @p metrics may be null, in which case no OpenTelemetry counters are recorded.
 */
future<StatusOr<OpenStreamResult>> HedgedOpenStream(
    CompletionQueue cq, OpenStreamFactory factory,
    google::storage::v2::BidiReadObjectRequest initial_request,
    std::shared_ptr<storage::AsyncRetryPolicy> retry_policy,
    HedgedOpenParams params,
    std::shared_ptr<storage::internal::HedgeLimiter> limiter,
    std::shared_ptr<storage::internal::HedgedReadMetrics> metrics,
    google::cloud::internal::ImmutableOptions options);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_HEDGED_OPEN_STREAM_H
