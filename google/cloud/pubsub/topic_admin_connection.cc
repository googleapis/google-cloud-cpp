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
#include "google/cloud/pubsub/internal/publisher_logging.h"
#include "google/cloud/pubsub/internal/publisher_metadata.h"
#include "google/cloud/pubsub/internal/publisher_stub.h"
#include "google/cloud/log.h"
#include "absl/strings/str_split.h"
#include <memory>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

namespace {
class TopicAdminConnectionImpl : public TopicAdminConnection {
 public:
  explicit TopicAdminConnectionImpl(
      std::shared_ptr<pubsub_internal::PublisherStub> stub)
      : stub_(std::move(stub)) {}

  ~TopicAdminConnectionImpl() override = default;

  StatusOr<google::pubsub::v1::Topic> CreateTopic(
      CreateTopicParams p) override {
    grpc::ClientContext context;
    return stub_->CreateTopic(context, p.topic);
  }

  StatusOr<google::pubsub::v1::Topic> GetTopic(GetTopicParams p) override {
    grpc::ClientContext context;
    google::pubsub::v1::GetTopicRequest request;
    request.set_topic(p.topic.FullName());
    return stub_->GetTopic(context, request);
  }

  StatusOr<google::pubsub::v1::Topic> UpdateTopic(
      UpdateTopicParams p) override {
    grpc::ClientContext context;
    return stub_->UpdateTopic(context, p.request);
  }

  ListTopicsRange ListTopics(ListTopicsParams p) override {
    google::pubsub::v1::ListTopicsRequest request;
    request.set_project(std::move(p.project_id));
    auto& stub = stub_;
    return ListTopicsRange(
        std::move(request),
        [stub](google::pubsub::v1::ListTopicsRequest const& request) {
          grpc::ClientContext context;
          return stub->ListTopics(context, request);
        },
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
    grpc::ClientContext context;
    return stub_->DeleteTopic(context, request);
  }

  StatusOr<google::pubsub::v1::DetachSubscriptionResponse> DetachSubscription(
      DetachSubscriptionParams p) override {
    google::pubsub::v1::DetachSubscriptionRequest request;
    request.set_subscription(p.subscription.FullName());
    grpc::ClientContext context;
    return stub_->DetachSubscription(context, request);
  }

  ListTopicSubscriptionsRange ListTopicSubscriptions(
      ListTopicSubscriptionsParams p) override {
    google::pubsub::v1::ListTopicSubscriptionsRequest request;
    request.set_topic(std::move(p.topic_full_name));
    auto& stub = stub_;
    return ListTopicSubscriptionsRange(
        std::move(request),
        [stub](
            google::pubsub::v1::ListTopicSubscriptionsRequest const& request) {
          grpc::ClientContext context;
          return stub->ListTopicSubscriptions(context, request);
        },
        [](google::pubsub::v1::ListTopicSubscriptionsResponse response) {
          std::vector<std::string> items;
          items.reserve(response.subscriptions_size());
          for (auto& item : *response.mutable_subscriptions()) {
            items.push_back(std::move(item));
          }
          return items;
        });
  }

  ListTopicSnapshotsRange ListTopicSnapshots(
      ListTopicSnapshotsParams p) override {
    google::pubsub::v1::ListTopicSnapshotsRequest request;
    request.set_topic(std::move(p.topic_full_name));
    auto& stub = stub_;
    return ListTopicSnapshotsRange(
        std::move(request),
        [stub](google::pubsub::v1::ListTopicSnapshotsRequest const& request) {
          grpc::ClientContext context;
          return stub->ListTopicSnapshots(context, request);
        },
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
};
}  // namespace

TopicAdminConnection::~TopicAdminConnection() = default;

std::shared_ptr<TopicAdminConnection> MakeTopicAdminConnection(
    ConnectionOptions const& options) {
  auto stub =
      pubsub_internal::CreateDefaultPublisherStub(options, /*channel_id=*/0);
  return pubsub_internal::MakeTopicAdminConnection(options, std::move(stub));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub

namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

std::shared_ptr<pubsub::TopicAdminConnection> MakeTopicAdminConnection(
    pubsub::ConnectionOptions const& options,
    std::shared_ptr<PublisherStub> stub) {
  stub = std::make_shared<pubsub_internal::PublisherMetadata>(std::move(stub));
  if (options.tracing_enabled("rpc")) {
    GCP_LOG(INFO) << "Enabled logging for gRPC calls";
    stub = std::make_shared<pubsub_internal::PublisherLogging>(
        std::move(stub), options.tracing_options());
  }
  return std::make_shared<pubsub::TopicAdminConnectionImpl>(std::move(stub));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
