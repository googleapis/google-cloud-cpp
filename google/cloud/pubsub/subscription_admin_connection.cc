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
#include "google/cloud/pubsub/internal/default_retry_policies.h"
#include "google/cloud/pubsub/internal/subscriber_logging.h"
#include "google/cloud/pubsub/internal/subscriber_metadata.h"
#include "google/cloud/internal/retry_loop.h"
#include "google/cloud/log.h"
#include <memory>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using google::cloud::internal::Idempotency;
using google::cloud::internal::RetryLoop;

class SubscriptionAdminConnectionImpl
    : public pubsub::SubscriptionAdminConnection {
 public:
  explicit SubscriptionAdminConnectionImpl(
      std::shared_ptr<pubsub_internal::SubscriberStub> stub,
      std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
      std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy)
      : stub_(std::move(stub)),
        retry_policy_(std::move(retry_policy)),
        backoff_policy_(std::move(backoff_policy)) {}

  ~SubscriptionAdminConnectionImpl() override = default;

  StatusOr<google::pubsub::v1::Subscription> CreateSubscription(
      CreateSubscriptionParams p) override {
    return RetryLoop(
        retry_policy_->clone(), backoff_policy_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               google::pubsub::v1::Subscription const& request) {
          return stub_->CreateSubscription(context, request);
        },
        p.subscription, __func__);
  }

  StatusOr<google::pubsub::v1::Subscription> GetSubscription(
      GetSubscriptionParams p) override {
    google::pubsub::v1::GetSubscriptionRequest request;
    request.set_subscription(p.subscription.FullName());
    return RetryLoop(
        retry_policy_->clone(), backoff_policy_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               google::pubsub::v1::GetSubscriptionRequest const& request) {
          return stub_->GetSubscription(context, request);
        },
        request, __func__);
  }

  StatusOr<google::pubsub::v1::Subscription> UpdateSubscription(
      UpdateSubscriptionParams p) override {
    return RetryLoop(
        retry_policy_->clone(), backoff_policy_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               google::pubsub::v1::UpdateSubscriptionRequest const& request) {
          return stub_->UpdateSubscription(context, request);
        },
        p.request, __func__);
  }

  pubsub::ListSubscriptionsRange ListSubscriptions(
      ListSubscriptionsParams p) override {
    google::pubsub::v1::ListSubscriptionsRequest request;
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
        [stub, retry, backoff, function_name](
            google::pubsub::v1::ListSubscriptionsRequest const& request) {
          return RetryLoop(
              retry->clone(), backoff->clone(), Idempotency::kIdempotent,
              [stub](grpc::ClientContext& c,
                     google::pubsub::v1::ListSubscriptionsRequest const& r) {
                return stub->ListSubscriptions(c, r);
              },
              request, function_name);
        };

    return pubsub::ListSubscriptionsRange(
        std::move(request), std::move(list_functor),
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
    return RetryLoop(
        retry_policy_->clone(), backoff_policy_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               google::pubsub::v1::DeleteSubscriptionRequest const& request) {
          return stub_->DeleteSubscription(context, request);
        },
        request, __func__);
  }

  Status ModifyPushConfig(ModifyPushConfigParams p) override {
    return RetryLoop(
        retry_policy_->clone(), backoff_policy_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               google::pubsub::v1::ModifyPushConfigRequest const& request) {
          return stub_->ModifyPushConfig(context, request);
        },
        p.request, __func__);
  }

  StatusOr<google::pubsub::v1::Snapshot> CreateSnapshot(
      CreateSnapshotParams p) override {
    auto const idempotency = p.request.name().empty()
                                 ? Idempotency::kNonIdempotent
                                 : Idempotency::kIdempotent;
    return RetryLoop(
        retry_policy_->clone(), backoff_policy_->clone(), idempotency,
        [this](grpc::ClientContext& context,
               google::pubsub::v1::CreateSnapshotRequest const& request) {
          return stub_->CreateSnapshot(context, request);
        },
        p.request, __func__);
  }

  StatusOr<google::pubsub::v1::Snapshot> GetSnapshot(
      GetSnapshotParams p) override {
    google::pubsub::v1::GetSnapshotRequest request;
    request.set_snapshot(p.snapshot.FullName());
    return RetryLoop(
        retry_policy_->clone(), backoff_policy_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               google::pubsub::v1::GetSnapshotRequest const& request) {
          return stub_->GetSnapshot(context, request);
        },
        request, __func__);
  }

  pubsub::ListSnapshotsRange ListSnapshots(ListSnapshotsParams p) override {
    google::pubsub::v1::ListSnapshotsRequest request;
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
        [stub, retry, backoff, function_name](
            google::pubsub::v1::ListSnapshotsRequest const& request) {
          return RetryLoop(
              retry->clone(), backoff->clone(), Idempotency::kIdempotent,
              [stub](grpc::ClientContext& c,
                     google::pubsub::v1::ListSnapshotsRequest const& r) {
                return stub->ListSnapshots(c, r);
              },
              request, function_name);
        };

    return pubsub::ListSnapshotsRange(
        std::move(request), std::move(list_functor),
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
    return RetryLoop(
        retry_policy_->clone(), backoff_policy_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               google::pubsub::v1::UpdateSnapshotRequest const& request) {
          return stub_->UpdateSnapshot(context, request);
        },
        p.request, __func__);
  }

  Status DeleteSnapshot(DeleteSnapshotParams p) override {
    google::pubsub::v1::DeleteSnapshotRequest request;
    request.set_snapshot(p.snapshot.FullName());
    return RetryLoop(
        retry_policy_->clone(), backoff_policy_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               google::pubsub::v1::DeleteSnapshotRequest const& request) {
          return stub_->DeleteSnapshot(context, request);
        },
        request, __func__);
  }

  StatusOr<google::pubsub::v1::SeekResponse> Seek(SeekParams p) override {
    return RetryLoop(
        retry_policy_->clone(), backoff_policy_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               google::pubsub::v1::SeekRequest const& request) {
          return stub_->Seek(context, request);
        },
        p.request, __func__);
  }

 private:
  std::shared_ptr<pubsub_internal::SubscriberStub> stub_;
  std::unique_ptr<pubsub::RetryPolicy const> retry_policy_;
  std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy_;
};
}  // namespace

std::shared_ptr<pubsub::SubscriptionAdminConnection>
MakeSubscriptionAdminConnection(
    pubsub::ConnectionOptions const& options,
    std::shared_ptr<SubscriberStub> stub,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy) {
  stub = std::make_shared<SubscriberMetadata>(std::move(stub));
  if (options.tracing_enabled("rpc")) {
    GCP_LOG(INFO) << "Enabled logging for gRPC calls";
    stub = std::make_shared<pubsub_internal::SubscriberLogging>(
        std::move(stub), options.tracing_options());
  }
  return std::make_shared<SubscriptionAdminConnectionImpl>(
      std::move(stub), std::move(retry_policy), std::move(backoff_policy));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal

namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

std::shared_ptr<SubscriptionAdminConnection> MakeSubscriptionAdminConnection(
    ConnectionOptions const& options,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy) {
  if (!retry_policy) retry_policy = pubsub_internal::DefaultRetryPolicy();
  if (!backoff_policy) backoff_policy = pubsub_internal::DefaultBackoffPolicy();
  return pubsub_internal::MakeSubscriptionAdminConnection(
      options,
      pubsub_internal::CreateDefaultSubscriberStub(options, /*channel_id=*/0),
      std::move(retry_policy), std::move(backoff_policy));
}

SubscriptionAdminConnection::~SubscriptionAdminConnection() = default;

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub

}  // namespace cloud
}  // namespace google
