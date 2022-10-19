// Copyright 2020 Google LLC
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

#include "google/cloud/pubsub/subscription_admin_connection.h"
#include "google/cloud/pubsub/internal/defaults.h"
#include "google/cloud/pubsub/internal/subscriber_auth_decorator.h"
#include "google/cloud/pubsub/internal/subscriber_logging_decorator.h"
#include "google/cloud/pubsub/internal/subscriber_metadata_decorator.h"
#include "google/cloud/pubsub/internal/subscriber_stub.h"
#include "google/cloud/pubsub/internal/subscriber_stub_factory.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/retry_loop.h"
#include "google/cloud/log.h"
#include <memory>

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::Idempotency;
using ::google::cloud::internal::RetryLoop;

class SubscriptionAdminConnectionImpl
    : public pubsub::SubscriptionAdminConnection {
 public:
  explicit SubscriptionAdminConnectionImpl(
      std::unique_ptr<google::cloud::BackgroundThreads> background,
      std::shared_ptr<pubsub_internal::SubscriberStub> stub, Options opts)
      : background_(std::move(background)),
        stub_(std::move(stub)),
        options_(std::move(opts)) {}

  ~SubscriptionAdminConnectionImpl() override = default;

  StatusOr<google::pubsub::v1::Subscription> CreateSubscription(
      CreateSubscriptionParams p) override {
    return RetryLoop(
        retry_policy(), backoff_policy(), Idempotency::kIdempotent,
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
        retry_policy(), backoff_policy(), Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               google::pubsub::v1::GetSubscriptionRequest const& request) {
          return stub_->GetSubscription(context, request);
        },
        request, __func__);
  }

  StatusOr<google::pubsub::v1::Subscription> UpdateSubscription(
      UpdateSubscriptionParams p) override {
    return RetryLoop(
        retry_policy(), backoff_policy(), Idempotency::kIdempotent,
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
    auto retry = std::shared_ptr<pubsub::RetryPolicy const>(retry_policy());
    auto backoff =
        std::shared_ptr<pubsub::BackoffPolicy const>(backoff_policy());
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

    return internal::MakePaginationRange<pubsub::ListSubscriptionsRange>(
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
        retry_policy(), backoff_policy(), Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               google::pubsub::v1::DeleteSubscriptionRequest const& request) {
          return stub_->DeleteSubscription(context, request);
        },
        request, __func__);
  }

  Status ModifyPushConfig(ModifyPushConfigParams p) override {
    return RetryLoop(
        retry_policy(), backoff_policy(), Idempotency::kIdempotent,
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
        retry_policy(), backoff_policy(), idempotency,
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
        retry_policy(), backoff_policy(), Idempotency::kIdempotent,
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
    auto retry = std::shared_ptr<pubsub::RetryPolicy const>(retry_policy());
    auto backoff =
        std::shared_ptr<pubsub::BackoffPolicy const>(backoff_policy());
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

    return internal::MakePaginationRange<pubsub::ListSnapshotsRange>(
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
        retry_policy(), backoff_policy(), Idempotency::kIdempotent,
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
        retry_policy(), backoff_policy(), Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               google::pubsub::v1::DeleteSnapshotRequest const& request) {
          return stub_->DeleteSnapshot(context, request);
        },
        request, __func__);
  }

  StatusOr<google::pubsub::v1::SeekResponse> Seek(SeekParams p) override {
    return RetryLoop(
        retry_policy(), backoff_policy(), Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               google::pubsub::v1::SeekRequest const& request) {
          return stub_->Seek(context, request);
        },
        p.request, __func__);
  }

  Options options() const override { return options_; }

 private:
  std::unique_ptr<pubsub::RetryPolicy> retry_policy() {
    auto const& options = internal::CurrentOptions();
    if (options.has<pubsub::RetryPolicyOption>()) {
      return options.get<pubsub::RetryPolicyOption>()->clone();
    }
    return options_.get<pubsub::RetryPolicyOption>()->clone();
  }

  std::unique_ptr<BackoffPolicy> backoff_policy() {
    auto const& options = internal::CurrentOptions();
    if (options.has<pubsub::BackoffPolicyOption>()) {
      return options.get<pubsub::BackoffPolicyOption>()->clone();
    }
    return options_.get<pubsub::BackoffPolicyOption>()->clone();
  }

  std::unique_ptr<google::cloud::BackgroundThreads> background_;
  std::shared_ptr<pubsub_internal::SubscriberStub> stub_;
  Options options_;
};

// Decorates a SubscriptionAdminStub. This works for both mock and real stubs.
std::shared_ptr<pubsub_internal::SubscriberStub> DecorateSubscriptionAdminStub(
    Options const& opts,
    std::shared_ptr<internal::GrpcAuthenticationStrategy> auth,
    std::shared_ptr<pubsub_internal::SubscriberStub> stub) {
  if (auth->RequiresConfigureContext()) {
    stub = std::make_shared<pubsub_internal::SubscriberAuth>(std::move(auth),
                                                             std::move(stub));
  }
  stub = std::make_shared<pubsub_internal::SubscriberMetadata>(std::move(stub));
  auto const& tracing = opts.get<TracingComponentsOption>();
  if (internal::Contains(tracing, "rpc")) {
    GCP_LOG(INFO) << "Enabled logging for gRPC calls";
    stub = std::make_shared<pubsub_internal::SubscriberLogging>(
        std::move(stub), opts.get<GrpcTracingOptionsOption>(), tracing);
  }
  return stub;
}

}  // namespace

SubscriptionAdminConnection::~SubscriptionAdminConnection() = default;

StatusOr<google::pubsub::v1::Subscription>
// NOLINTNEXTLINE(performance-unnecessary-value-param)
SubscriptionAdminConnection::CreateSubscription(CreateSubscriptionParams) {
  return Status{StatusCode::kUnimplemented, "needs-override"};
}

StatusOr<google::pubsub::v1::Subscription>
// NOLINTNEXTLINE(performance-unnecessary-value-param)
SubscriptionAdminConnection::GetSubscription(GetSubscriptionParams) {
  return Status{StatusCode::kUnimplemented, "needs-override"};
}

StatusOr<google::pubsub::v1::Subscription>
// NOLINTNEXTLINE(performance-unnecessary-value-param)
SubscriptionAdminConnection::UpdateSubscription(UpdateSubscriptionParams) {
  return Status{StatusCode::kUnimplemented, "needs-override"};
}

ListSubscriptionsRange SubscriptionAdminConnection::ListSubscriptions(
    ListSubscriptionsParams) {  // NOLINT(performance-unnecessary-value-param)
  return internal::MakeUnimplementedPaginationRange<ListSubscriptionsRange>();
}

Status SubscriptionAdminConnection::DeleteSubscription(
    DeleteSubscriptionParams) {  // NOLINT(performance-unnecessary-value-param)
  return Status{StatusCode::kUnimplemented, "needs-override"};
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
Status SubscriptionAdminConnection::ModifyPushConfig(ModifyPushConfigParams) {
  return Status{StatusCode::kUnimplemented, "needs-override"};
}

StatusOr<google::pubsub::v1::Snapshot>
// NOLINTNEXTLINE(performance-unnecessary-value-param)
SubscriptionAdminConnection::CreateSnapshot(CreateSnapshotParams) {
  return Status{StatusCode::kUnimplemented, "needs-override"};
}

StatusOr<google::pubsub::v1::Snapshot> SubscriptionAdminConnection::GetSnapshot(
    GetSnapshotParams) {  // NOLINT(performance-unnecessary-value-param)
  return Status{StatusCode::kUnimplemented, "needs-override"};
}

StatusOr<google::pubsub::v1::Snapshot>
// NOLINTNEXTLINE(performance-unnecessary-value-param)
SubscriptionAdminConnection::UpdateSnapshot(UpdateSnapshotParams) {
  return Status{StatusCode::kUnimplemented, "needs-override"};
}

ListSnapshotsRange SubscriptionAdminConnection::ListSnapshots(
    ListSnapshotsParams) {  // NOLINT(performance-unnecessary-value-param)
  return internal::MakeUnimplementedPaginationRange<ListSnapshotsRange>();
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
Status SubscriptionAdminConnection::DeleteSnapshot(DeleteSnapshotParams) {
  return Status{StatusCode::kUnimplemented, "needs-override"};
}

StatusOr<google::pubsub::v1::SeekResponse> SubscriptionAdminConnection::Seek(
    SeekParams) {  // NOLINT(performance-unnecessary-value-param)
  return Status{StatusCode::kUnimplemented, "needs-override"};
}

std::shared_ptr<SubscriptionAdminConnection> MakeSubscriptionAdminConnection(
    std::initializer_list<internal::NonConstructible>) {
  return MakeSubscriptionAdminConnection();
}

std::shared_ptr<SubscriptionAdminConnection> MakeSubscriptionAdminConnection(
    Options opts) {
  internal::CheckExpectedOptions<CommonOptionList, GrpcOptionList,
                                 UnifiedCredentialsOptionList,
                                 PolicyOptionList>(opts, __func__);
  opts = pubsub_internal::DefaultCommonOptions(std::move(opts));
  auto background = internal::MakeBackgroundThreadsFactory(opts)();
  auto auth = google::cloud::internal::CreateAuthenticationStrategy(
      background->cq(), opts);
  auto stub = pubsub_internal::CreateDefaultSubscriberStub(auth->CreateChannel(
      opts.get<EndpointOption>(), internal::MakeChannelArguments(opts)));
  stub = DecorateSubscriptionAdminStub(opts, std::move(auth), std::move(stub));
  return std::make_shared<SubscriptionAdminConnectionImpl>(
      std::move(background), std::move(stub), std::move(opts));
}

std::shared_ptr<SubscriptionAdminConnection> MakeSubscriptionAdminConnection(
    pubsub::ConnectionOptions const& options,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy) {
  auto opts = internal::MakeOptions(options);
  if (retry_policy) opts.set<RetryPolicyOption>(retry_policy->clone());
  if (backoff_policy) opts.set<BackoffPolicyOption>(backoff_policy->clone());
  return MakeSubscriptionAdminConnection(std::move(opts));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub

namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::shared_ptr<pubsub::SubscriptionAdminConnection>
MakeTestSubscriptionAdminConnection(Options const& opts,
                                    std::shared_ptr<SubscriberStub> stub) {
  auto background = internal::MakeBackgroundThreadsFactory(opts)();
  auto auth = google::cloud::internal::CreateAuthenticationStrategy(
      background->cq(), opts);
  stub = pubsub::DecorateSubscriptionAdminStub(opts, std::move(auth),
                                               std::move(stub));
  return std::make_shared<pubsub::SubscriptionAdminConnectionImpl>(
      std::move(background), std::move(stub), opts);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
