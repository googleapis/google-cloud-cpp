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

#include "google/cloud/pubsub/subscription_admin_connection.h"
#include "google/cloud/pubsub/internal/subscriber_logging.h"
#include "google/cloud/log.h"
#include <memory>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

std::shared_ptr<SubscriptionAdminConnection> MakeSubscriptionAdminConnection(
    ConnectionOptions const& options) {
  return pubsub_internal::MakeSubscriptionAdminConnection(
      options,
      pubsub_internal::CreateDefaultSubscriberStub(options, /*channel_id=*/0));
}

SubscriptionAdminConnection::~SubscriptionAdminConnection() = default;

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub

namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

namespace {
class SubscriptionAdminConnectionImpl
    : public pubsub::SubscriptionAdminConnection {
 public:
  explicit SubscriptionAdminConnectionImpl(
      std::shared_ptr<pubsub_internal::SubscriberStub> stub)
      : stub_(std::move(stub)) {}

  ~SubscriptionAdminConnectionImpl() override = default;

  StatusOr<google::pubsub::v1::Subscription> CreateSubscription(
      CreateSubscriptionParams p) override {
    grpc::ClientContext context;
    return stub_->CreateSubscription(context, p.subscription);
  }

  StatusOr<google::pubsub::v1::Subscription> GetSubscription(
      GetSubscriptionParams p) override {
    google::pubsub::v1::GetSubscriptionRequest request;
    request.set_subscription(p.subscription.FullName());
    grpc::ClientContext context;
    return stub_->GetSubscription(context, request);
  }

  StatusOr<google::pubsub::v1::Subscription> UpdateSubscription(
      UpdateSubscriptionParams p) override {
    grpc::ClientContext context;
    return stub_->UpdateSubscription(context, p.request);
  }

  pubsub::ListSubscriptionsRange ListSubscriptions(
      ListSubscriptionsParams p) override {
    google::pubsub::v1::ListSubscriptionsRequest request;
    request.set_project(std::move(p.project_id));
    auto& stub = stub_;
    return pubsub::ListSubscriptionsRange(
        std::move(request),
        [stub](google::pubsub::v1::ListSubscriptionsRequest const& request) {
          grpc::ClientContext context;
          return stub->ListSubscriptions(context, request);
        },
        [](google::pubsub::v1::ListSubscriptionsResponse response) {
          std::vector<google::pubsub::v1::Subscription> items;
          items.reserve(response.subscriptions_size());
          for (auto& item : *response.mutable_subscriptions()) {
            items.push_back(std::move(item));
          }
          return items;
        });
  }

  Status DeleteSubscription(DeleteSubscriptionParams p) override {
    google::pubsub::v1::DeleteSubscriptionRequest request;
    request.set_subscription(p.subscription.FullName());
    grpc::ClientContext context;
    return stub_->DeleteSubscription(context, request);
  }

  Status ModifyPushConfig(ModifyPushConfigParams p) override {
    grpc::ClientContext context;
    return stub_->ModifyPushConfig(context, p.request);
  }

  StatusOr<google::pubsub::v1::Snapshot> CreateSnapshot(
      CreateSnapshotParams p) override {
    grpc::ClientContext context;
    return stub_->CreateSnapshot(context, std::move(p.request));
  }

  StatusOr<google::pubsub::v1::Snapshot> GetSnapshot(
      GetSnapshotParams p) override {
    google::pubsub::v1::GetSnapshotRequest request;
    request.set_snapshot(p.snapshot.FullName());
    grpc::ClientContext context;
    return stub_->GetSnapshot(context, request);
  }

  pubsub::ListSnapshotsRange ListSnapshots(ListSnapshotsParams p) override {
    google::pubsub::v1::ListSnapshotsRequest request;
    request.set_project(std::move(p.project_id));
    auto& stub = stub_;
    return pubsub::ListSnapshotsRange(
        std::move(request),
        [stub](google::pubsub::v1::ListSnapshotsRequest const& request) {
          grpc::ClientContext context;
          return stub->ListSnapshots(context, request);
        },
        [](google::pubsub::v1::ListSnapshotsResponse response) {
          std::vector<google::pubsub::v1::Snapshot> items;
          items.reserve(response.snapshots_size());
          for (auto& item : *response.mutable_snapshots()) {
            items.push_back(std::move(item));
          }
          return items;
        });
  }

  StatusOr<google::pubsub::v1::Snapshot> UpdateSnapshot(
      UpdateSnapshotParams p) override {
    grpc::ClientContext context;
    return stub_->UpdateSnapshot(context, std::move(p.request));
  }

  Status DeleteSnapshot(DeleteSnapshotParams p) override {
    google::pubsub::v1::DeleteSnapshotRequest request;
    request.set_snapshot(p.snapshot.FullName());
    grpc::ClientContext context;
    return stub_->DeleteSnapshot(context, request);
  }

  StatusOr<google::pubsub::v1::SeekResponse> Seek(SeekParams p) override {
    grpc::ClientContext context;
    return stub_->Seek(context, std::move(p.request));
  }

 private:
  std::shared_ptr<pubsub_internal::SubscriberStub> stub_;
};
}  // namespace

std::shared_ptr<pubsub::SubscriptionAdminConnection>
MakeSubscriptionAdminConnection(pubsub::ConnectionOptions const& options,
                                std::shared_ptr<SubscriberStub> stub) {
  if (options.tracing_enabled("rpc")) {
    GCP_LOG(INFO) << "Enabled logging for gRPC calls";
    stub = std::make_shared<pubsub_internal::SubscriberLogging>(
        std::move(stub), options.tracing_options());
  }
  return std::make_shared<SubscriptionAdminConnectionImpl>(std::move(stub));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
