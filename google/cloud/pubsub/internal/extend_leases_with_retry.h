// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_EXTEND_LEASES_WITH_RETRY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_EXTEND_LEASES_WITH_RETRY_H

#include "google/cloud/pubsub/backoff_policy.h"
#include "google/cloud/pubsub/internal/batch_callback.h"
#include "google/cloud/pubsub/internal/subscriber_stub.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/future.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include <google/pubsub/v1/pubsub.pb.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Extend a number of leases, asynchronously retrying on transient failures.
 *
 * The Pub/Sub C++ client library automatically extends the leases for messages
 * that are still being processed by the application.  Normally these extensions
 * are best-effort, the library makes a single attempt to extend the lease. For
 * exactly-once delivery we make more than one such attempt, and we need to
 * handle partial failures.
 */
future<Status> ExtendLeasesWithRetry(
    std::shared_ptr<SubscriberStub> stub, CompletionQueue cq,
    google::pubsub::v1::ModifyAckDeadlineRequest request,
    std::shared_ptr<BatchCallback> callback, bool enable_otel);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_EXTEND_LEASES_WITH_RETRY_H
