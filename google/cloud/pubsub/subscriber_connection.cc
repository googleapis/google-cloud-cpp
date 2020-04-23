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

#include "google/cloud/pubsub/subscriber_connection.h"
#include "google/cloud/pubsub/internal/subscriber_stub.h"
#include <memory>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

namespace {
class SubscriberConnectionImpl : public SubscriberConnection {
 public:
  explicit SubscriberConnectionImpl(
      std::shared_ptr<pubsub_internal::SubscriberStub> stub)
      : stub_(std::move(stub)) {}

  ~SubscriberConnectionImpl() override = default;

  StatusOr<google::pubsub::v1::Subscription> CreateSubscription(
      CreateSubscriptionParams p) override {
    grpc::ClientContext context;
    return stub_->CreateSubscription(context, p.subscription);
  }

  ListSubscriptionsRange ListSubscriptions(ListSubscriptionsParams p) override {
    google::pubsub::v1::ListSubscriptionsRequest request;
    request.set_project(std::move(p.project_id));
    auto& stub = stub_;
    return ListSubscriptionsRange(
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

 private:
  std::shared_ptr<pubsub_internal::SubscriberStub> stub_;
};
}  // namespace

SubscriberConnection::~SubscriberConnection() = default;

std::shared_ptr<SubscriberConnection> MakeSubscriberConnection(
    ConnectionOptions const& options) {
  auto stub =
      pubsub_internal::CreateDefaultSubscriberStub(options, /*channel_id=*/0);
  return std::make_shared<SubscriberConnectionImpl>(std::move(stub));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
