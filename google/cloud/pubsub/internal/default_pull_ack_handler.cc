// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/pubsub/internal/default_pull_ack_handler.h"
#include "google/cloud/pubsub/internal/exactly_once_policies.h"
#include "google/cloud/pubsub/internal/pull_lease_manager.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/internal/async_retry_loop.h"
#include "absl/memory/memory.h"
#include <google/pubsub/v1/pubsub.pb.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

DefaultPullAckHandler::DefaultPullAckHandler(CompletionQueue cq,
                                             std::weak_ptr<SubscriberStub> w,
                                             Options options,
                                             pubsub::Subscription subscription,
                                             std::string ack_id,
                                             std::int32_t delivery_attempt)
    : cq_(std::move(cq)),
      stub_(std::move(w)),
      subscription_(std::move(subscription)),
      ack_id_(std::move(ack_id)),
      delivery_attempt_(delivery_attempt),
      lease_manager_(std::make_shared<PullLeaseManager>(
          cq_, stub_, std::move(options), subscription_, ack_id_)) {}

DefaultPullAckHandler::~DefaultPullAckHandler() = default;

future<Status> DefaultPullAckHandler::ack() {
  if (auto s = stub_.lock()) {
    google::pubsub::v1::AcknowledgeRequest request;
    request.set_subscription(subscription_.FullName());
    request.add_ack_ids(ack_id_);
    return internal::AsyncRetryLoop(
        absl::make_unique<ExactlyOnceRetryPolicy>(ack_id_),
        ExactlyOnceBackoffPolicy(), google::cloud::Idempotency::kIdempotent,
        cq_,
        [stub = std::move(s)](auto cq, auto context, auto const& request) {
          return stub->AsyncAcknowledge(cq, std::move(context), request);
        },
        request, __func__);
  }
  return make_ready_future(
      Status(StatusCode::kFailedPrecondition, "session already shutdown"));
}

future<Status> DefaultPullAckHandler::nack() {
  if (auto s = stub_.lock()) {
    google::pubsub::v1::ModifyAckDeadlineRequest request;
    request.set_subscription(subscription_.FullName());
    request.set_ack_deadline_seconds(0);
    request.add_ack_ids(ack_id_);
    return internal::AsyncRetryLoop(
        absl::make_unique<ExactlyOnceRetryPolicy>(ack_id_),
        ExactlyOnceBackoffPolicy(), google::cloud::Idempotency::kIdempotent,
        cq_,
        [stub = std::move(s)](auto cq, auto context, auto const& request) {
          return stub->AsyncModifyAckDeadline(cq, std::move(context), request);
        },
        request, __func__);
  }
  return make_ready_future(
      Status(StatusCode::kFailedPrecondition, "session already shutdown"));
}

std::int32_t DefaultPullAckHandler::delivery_attempt() const {
  return delivery_attempt_;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
