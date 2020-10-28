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

#include "google/cloud/pubsub/topic_admin_connection.h"
#include "google/cloud/pubsub/internal/default_retry_policies.h"
#include "google/cloud/pubsub/internal/publisher_logging.h"
#include "google/cloud/pubsub/internal/publisher_metadata.h"
#include "google/cloud/pubsub/internal/publisher_stub.h"
#include "google/cloud/internal/retry_loop.h"
#include "google/cloud/log.h"
#include "absl/strings/str_split.h"
#include <memory>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using google::cloud::internal::Idempotency;
using google::cloud::internal::RetryLoop;

class TopicAdminConnectionImpl : public pubsub::TopicAdminConnection {
 public:
  explicit TopicAdminConnectionImpl(
      std::shared_ptr<pubsub_internal::PublisherStub> stub,
      std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
      std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy)
      : stub_(std::move(stub)),
        retry_policy_(std::move(retry_policy)),
        backoff_policy_(std::move(backoff_policy)) {}

  ~TopicAdminConnectionImpl() override = default;

  StatusOr<google::pubsub::v1::Topic> CreateTopic(
      CreateTopicParams p) override {
    return RetryLoop(
        retry_policy_->clone(), backoff_policy_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               google::pubsub::v1::Topic const& request) {
          return stub_->CreateTopic(context, request);
        },
        p.topic, __func__);
  }

  StatusOr<google::pubsub::v1::Topic> GetTopic(GetTopicParams p) override {
    google::pubsub::v1::GetTopicRequest request;
    request.set_topic(p.topic.FullName());
    return RetryLoop(
        retry_policy_->clone(), backoff_policy_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               google::pubsub::v1::GetTopicRequest const& request) {
          return stub_->GetTopic(context, request);
        },
        request, __func__);
  }

  StatusOr<google::pubsub::v1::Topic> UpdateTopic(
      UpdateTopicParams p) override {
    return RetryLoop(
        retry_policy_->clone(), backoff_policy_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               google::pubsub::v1::UpdateTopicRequest const& request) {
          return stub_->UpdateTopic(context, request);
        },
        p.request, __func__);
  }

  pubsub::ListTopicsRange ListTopics(ListTopicsParams p) override {
    google::pubsub::v1::ListTopicsRequest request;
    request.set_project(std::move(p.project_id));
    auto& stub = stub_;
    // Because we do not have C++14 generalized lambda captures we cannot just
    // use the unique_ptr<> here, so convert to shared_ptr<> instead.
    auto retry =
        std::shared_ptr<pubsub::RetryPolicy const>(retry_policy_->clone());
    auto backoff =
        std::shared_ptr<pubsub::BackoffPolicy const>(backoff_policy_->clone());
    char const* function_name = __func__;
    auto list_functor =
        [stub, retry, backoff,
         function_name](google::pubsub::v1::ListTopicsRequest const& request) {
          return RetryLoop(
              retry->clone(), backoff->clone(), Idempotency::kIdempotent,
              [stub](grpc::ClientContext& c,
                     google::pubsub::v1::ListTopicsRequest const& r) {
                return stub->ListTopics(c, r);
              },
              request, function_name);
        };
    return pubsub::ListTopicsRange(
        std::move(request), list_functor,
        [](google::pubsub::v1::ListTopicsResponse response) {
          std::vector<google::pubsub::v1::Topic> items;
          items.reserve(response.topics_size());
          for (auto& item : *response.mutable_topics()) {
            items.push_back(std::move(item));
          }
          return items;
        });
  }

  Status DeleteTopic(DeleteTopicParams p) override {
    google::pubsub::v1::DeleteTopicRequest request;
    request.set_topic(p.topic.FullName());
    return RetryLoop(
        retry_policy_->clone(), backoff_policy_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               google::pubsub::v1::DeleteTopicRequest const& request) {
          return stub_->DeleteTopic(context, request);
        },
        request, __func__);
  }

  StatusOr<google::pubsub::v1::DetachSubscriptionResponse> DetachSubscription(
      DetachSubscriptionParams p) override {
    google::pubsub::v1::DetachSubscriptionRequest request;
    request.set_subscription(p.subscription.FullName());
    grpc::ClientContext context;
    return RetryLoop(
        retry_policy_->clone(), backoff_policy_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               google::pubsub::v1::DetachSubscriptionRequest const& request) {
          return stub_->DetachSubscription(context, request);
        },
        request, __func__);
  }

  pubsub::ListTopicSubscriptionsRange ListTopicSubscriptions(
      ListTopicSubscriptionsParams p) override {
    google::pubsub::v1::ListTopicSubscriptionsRequest request;
    request.set_topic(std::move(p.topic_full_name));
    auto& stub = stub_;
    // Because we do not have C++14 generalized lambda captures we cannot just
    // use the unique_ptr<> here, so convert to shared_ptr<> instead.
    auto retry =
        std::shared_ptr<pubsub::RetryPolicy const>(retry_policy_->clone());
    auto backoff =
        std::shared_ptr<pubsub::BackoffPolicy const>(backoff_policy_->clone());
    char const* function_name = __func__;
    auto list_functor =
        [stub, retry, backoff, function_name](
            google::pubsub::v1::ListTopicSubscriptionsRequest const& request) {
          return RetryLoop(
              retry->clone(), backoff->clone(), Idempotency::kIdempotent,
              [stub](
                  grpc::ClientContext& c,
                  google::pubsub::v1::ListTopicSubscriptionsRequest const& r) {
                return stub->ListTopicSubscriptions(c, r);
              },
              request, function_name);
        };
    return pubsub::ListTopicSubscriptionsRange(
        std::move(request), std::move(list_functor),
        [](google::pubsub::v1::ListTopicSubscriptionsResponse response) {
          std::vector<std::string> items;
          items.reserve(response.subscriptions_size());
          for (auto& item : *response.mutable_subscriptions()) {
            items.push_back(std::move(item));
          }
          return items;
        });
  }

  pubsub::ListTopicSnapshotsRange ListTopicSnapshots(
      ListTopicSnapshotsParams p) override {
    google::pubsub::v1::ListTopicSnapshotsRequest request;
    request.set_topic(std::move(p.topic_full_name));
    auto& stub = stub_;
    // Because we do not have C++14 generalized lambda captures we cannot just
    // use the unique_ptr<> here, so convert to shared_ptr<> instead.
    auto retry =
        std::shared_ptr<pubsub::RetryPolicy const>(retry_policy_->clone());
    auto backoff =
        std::shared_ptr<pubsub::BackoffPolicy const>(backoff_policy_->clone());
    char const* function_name = __func__;
    auto list_functor =
        [stub, retry, backoff, function_name](
            google::pubsub::v1::ListTopicSnapshotsRequest const& request) {
          return RetryLoop(
              retry->clone(), backoff->clone(), Idempotency::kIdempotent,
              [stub](grpc::ClientContext& c,
                     google::pubsub::v1::ListTopicSnapshotsRequest const& r) {
                return stub->ListTopicSnapshots(c, r);
              },
              request, function_name);
        };
    return pubsub::ListTopicSnapshotsRange(
        std::move(request), list_functor,
        [](google::pubsub::v1::ListTopicSnapshotsResponse response) {
          std::vector<std::string> items;
          items.reserve(response.snapshots_size());
          for (auto& item : *response.mutable_snapshots()) {
            items.push_back(std::move(item));
          }
          return items;
        });
  }

 private:
  std::shared_ptr<pubsub_internal::PublisherStub> stub_;
  std::unique_ptr<pubsub::RetryPolicy const> retry_policy_;
  std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy_;
};
}  // namespace

std::shared_ptr<pubsub::TopicAdminConnection> MakeTopicAdminConnection(
    pubsub::ConnectionOptions const& options,
    std::shared_ptr<PublisherStub> stub,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy) {
  stub = std::make_shared<pubsub_internal::PublisherMetadata>(std::move(stub));
  if (options.tracing_enabled("rpc")) {
    GCP_LOG(INFO) << "Enabled logging for gRPC calls";
    stub = std::make_shared<pubsub_internal::PublisherLogging>(
        std::move(stub), options.tracing_options());
  }
  return std::make_shared<TopicAdminConnectionImpl>(
      std::move(stub), std::move(retry_policy), std::move(backoff_policy));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal

namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

TopicAdminConnection::~TopicAdminConnection() = default;

StatusOr<google::pubsub::v1::Topic> TopicAdminConnection::CreateTopic(
    CreateTopicParams) {  // NOLINT(performance-unnecessary-value-param)
  return Status{StatusCode::kUnimplemented, "needs-override"};
}

StatusOr<google::pubsub::v1::Topic> TopicAdminConnection::GetTopic(
    GetTopicParams) {  // NOLINT(performance-unnecessary-value-param)
  return Status{StatusCode::kUnimplemented, "needs-override"};
}

StatusOr<google::pubsub::v1::Topic> TopicAdminConnection::UpdateTopic(
    UpdateTopicParams) {  // NOLINT(performance-unnecessary-value-param)
  return Status{StatusCode::kUnimplemented, "needs-override"};
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
ListTopicsRange TopicAdminConnection::ListTopics(ListTopicsParams) {
  return internal::UnimplementedPaginationRange<ListTopicsRange>::Create();
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
Status TopicAdminConnection::DeleteTopic(DeleteTopicParams) {
  return Status{StatusCode::kUnimplemented, "needs-override"};
}

StatusOr<google::pubsub::v1::DetachSubscriptionResponse>
// NOLINTNEXTLINE(performance-unnecessary-value-param)
TopicAdminConnection::DetachSubscription(DetachSubscriptionParams) {
  return Status{StatusCode::kUnimplemented, "needs-override"};
}

ListTopicSubscriptionsRange TopicAdminConnection::ListTopicSubscriptions(
    ListTopicSubscriptionsParams) {  // NOLINT(performance-unnecessary-value-param)
  return internal::UnimplementedPaginationRange<
      ListTopicSubscriptionsRange>::Create();
}

ListTopicSnapshotsRange TopicAdminConnection::ListTopicSnapshots(
    ListTopicSnapshotsParams) {  // NOLINT(performance-unnecessary-value-param)

  return internal::UnimplementedPaginationRange<
      ListTopicSnapshotsRange>::Create();
}

std::shared_ptr<TopicAdminConnection> MakeTopicAdminConnection(
    ConnectionOptions const& options,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy) {
  auto stub =
      pubsub_internal::CreateDefaultPublisherStub(options, /*channel_id=*/0);
  if (!retry_policy) retry_policy = pubsub_internal::DefaultRetryPolicy();
  if (!backoff_policy) backoff_policy = pubsub_internal::DefaultBackoffPolicy();
  return pubsub_internal::MakeTopicAdminConnection(options, std::move(stub),
                                                   std::move(retry_policy),
                                                   std::move(backoff_policy));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
