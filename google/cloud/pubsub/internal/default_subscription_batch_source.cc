// Copyright 2020 Google LLC
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

#include "google/cloud/pubsub/internal/default_subscription_batch_source.h"
#include "google/cloud/internal/async_retry_loop.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

using google::cloud::internal::AsyncRetryLoop;
using google::cloud::internal::Idempotency;

void DefaultSubscriptionBatchSource::Shutdown() {}

future<Status> DefaultSubscriptionBatchSource::AckMessage(
    std::string const& ack_id, std::size_t) {
  google::pubsub::v1::AcknowledgeRequest request;
  request.set_subscription(subscription_full_name_);
  request.add_ack_ids(ack_id);
  auto& stub = stub_;
  return AsyncRetryLoop(
      retry_policy_->clone(), backoff_policy_->clone(),
      Idempotency::kIdempotent, cq_,
      [stub](google::cloud::CompletionQueue& cq,
             std::unique_ptr<grpc::ClientContext> context,
             google::pubsub::v1::AcknowledgeRequest const& request) {
        return stub->AsyncAcknowledge(cq, std::move(context), request);
      },
      request, __func__);
}

future<Status> DefaultSubscriptionBatchSource::NackMessage(
    std::string const& ack_id, std::size_t) {
  google::pubsub::v1::ModifyAckDeadlineRequest request;
  request.set_subscription(subscription_full_name_);
  request.add_ack_ids(ack_id);
  request.set_ack_deadline_seconds(0);
  auto& stub = stub_;
  return AsyncRetryLoop(
      retry_policy_->clone(), backoff_policy_->clone(),
      Idempotency::kIdempotent, cq_,
      [stub](google::cloud::CompletionQueue& cq,
             std::unique_ptr<grpc::ClientContext> context,
             google::pubsub::v1::ModifyAckDeadlineRequest const& request) {
        return stub->AsyncModifyAckDeadline(cq, std::move(context), request);
      },
      request, __func__);
}

future<Status> DefaultSubscriptionBatchSource::BulkNack(
    std::vector<std::string> ack_ids, std::size_t) {
  google::pubsub::v1::ModifyAckDeadlineRequest request;
  request.set_subscription(subscription_full_name_);
  for (auto& id : ack_ids) {
    request.add_ack_ids(std::move(id));
  }
  request.set_ack_deadline_seconds(0);
  auto& stub = stub_;
  return AsyncRetryLoop(
      retry_policy_->clone(), backoff_policy_->clone(),
      Idempotency::kIdempotent, cq_,
      [stub](google::cloud::CompletionQueue& cq,
             std::unique_ptr<grpc::ClientContext> context,
             google::pubsub::v1::ModifyAckDeadlineRequest const& request) {
        return stub->AsyncModifyAckDeadline(cq, std::move(context), request);
      },
      request, __func__);
}

future<Status> DefaultSubscriptionBatchSource::ExtendLeases(
    std::vector<std::string> ack_ids, std::chrono::seconds extension) {
  google::pubsub::v1::ModifyAckDeadlineRequest request;
  request.set_subscription(subscription_full_name_);
  auto const actual_extension = [extension] {
    // The service does not allow extending the deadline by more than 10
    // minutes.
    auto constexpr kMaximumAckDeadline = std::chrono::seconds(600);
    if (extension < std::chrono::seconds(0)) return std::chrono::seconds(0);
    if (extension > kMaximumAckDeadline) return kMaximumAckDeadline;
    return extension;
  }();
  request.set_ack_deadline_seconds(
      static_cast<std::int32_t>(actual_extension.count()));
  for (auto& a : ack_ids) request.add_ack_ids(std::move(a));
  auto& stub = stub_;
  return AsyncRetryLoop(
      retry_policy_->clone(), backoff_policy_->clone(),
      Idempotency::kIdempotent, cq_,
      [stub](google::cloud::CompletionQueue& cq,
             std::unique_ptr<grpc::ClientContext> context,
             google::pubsub::v1::ModifyAckDeadlineRequest const& request) {
        return stub->AsyncModifyAckDeadline(cq, std::move(context), request);
      },
      request, __func__);
}

future<StatusOr<google::pubsub::v1::PullResponse>>
DefaultSubscriptionBatchSource::Pull(std::int32_t max_count) {
  google::pubsub::v1::PullRequest request;
  request.set_subscription(subscription_full_name_);
  request.set_max_messages(max_count);

  auto& stub = stub_;
  return AsyncRetryLoop(
      retry_policy_->clone(), backoff_policy_->clone(),
      Idempotency::kIdempotent, cq_,
      [stub](google::cloud::CompletionQueue& cq,
             std::unique_ptr<grpc::ClientContext> context,
             google::pubsub::v1::PullRequest const& request) {
        return stub->AsyncPull(cq, std::move(context), request);
      },
      request, __func__);
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
