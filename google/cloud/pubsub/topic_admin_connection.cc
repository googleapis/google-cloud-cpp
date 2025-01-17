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

// TODO(#12987): Remove this file after the deprecation period expires
#include "google/cloud/internal/disable_deprecation_warnings.inc"
#include "google/cloud/pubsub/topic_admin_connection.h"
#include "google/cloud/pubsub/internal/create_channel.h"
#include "google/cloud/pubsub/internal/defaults.h"
#include "google/cloud/pubsub/internal/publisher_stub_factory.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/retry_loop.h"
#include "google/cloud/log.h"
#include "absl/strings/str_split.h"
#include <initializer_list>
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::Idempotency;
using ::google::cloud::internal::RetryLoop;

std::unique_ptr<google::cloud::RetryPolicy> retry_policy(
    Options const& options) {
  return options.get<pubsub::RetryPolicyOption>()->clone();
}

std::unique_ptr<BackoffPolicy> backoff_policy(Options const& options) {
  return options.get<pubsub::BackoffPolicyOption>()->clone();
}

class TopicAdminConnectionImpl : public pubsub::TopicAdminConnection {
 public:
  explicit TopicAdminConnectionImpl(
      std::unique_ptr<google::cloud::BackgroundThreads> background,
      std::shared_ptr<pubsub_internal::PublisherStub> stub, Options opts)
      : background_(std::move(background)),
        stub_(std::move(stub)),
        options_(std::move(opts)) {}

  ~TopicAdminConnectionImpl() override = default;

  StatusOr<google::pubsub::v1::Topic> CreateTopic(
      CreateTopicParams p) override {
    auto const current = google::cloud::internal::SaveCurrentOptions();
    return RetryLoop(
        retry_policy(*current), backoff_policy(*current),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context, Options const& options,
               google::pubsub::v1::Topic const& request) {
          return stub_->CreateTopic(context, options, request);
        },
        *current, p.topic, __func__);
  }

  StatusOr<google::pubsub::v1::Topic> GetTopic(GetTopicParams p) override {
    auto const current = google::cloud::internal::SaveCurrentOptions();
    google::pubsub::v1::GetTopicRequest request;
    request.set_topic(p.topic.FullName());
    return RetryLoop(
        retry_policy(*current), backoff_policy(*current),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context, Options const& options,
               google::pubsub::v1::GetTopicRequest const& request) {
          return stub_->GetTopic(context, options, request);
        },
        *current, request, __func__);
  }

  StatusOr<google::pubsub::v1::Topic> UpdateTopic(
      UpdateTopicParams p) override {
    auto const current = google::cloud::internal::SaveCurrentOptions();
    return RetryLoop(
        retry_policy(*current), backoff_policy(*current),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context, Options const& options,
               google::pubsub::v1::UpdateTopicRequest const& request) {
          return stub_->UpdateTopic(context, options, request);
        },
        *current, p.request, __func__);
  }

  pubsub::ListTopicsRange ListTopics(ListTopicsParams p) override {
    auto current = google::cloud::internal::SaveCurrentOptions();
    google::pubsub::v1::ListTopicsRequest request;
    request.set_project(std::move(p.project_id));
    // Because we do not have C++14 generalized lambda captures we cannot just
    // use the unique_ptr<> here, so convert to shared_ptr<> instead.
    auto retry = std::shared_ptr<google::cloud::RetryPolicy const>(
        retry_policy(*current));
    auto backoff =
        std::shared_ptr<pubsub::BackoffPolicy const>(backoff_policy(*current));
    char const* function_name = __func__;
    auto list_functor =
        [stub = stub_, retry = std::move(retry), backoff = std::move(backoff),
         function_name](Options const& options,
                        google::pubsub::v1::ListTopicsRequest const& request) {
          return RetryLoop(
              retry->clone(), backoff->clone(), Idempotency::kIdempotent,
              [stub](grpc::ClientContext& c, Options const& o,
                     google::pubsub::v1::ListTopicsRequest const& r) {
                return stub->ListTopics(c, o, r);
              },
              options, request, function_name);
        };
    return internal::MakePaginationRange<pubsub::ListTopicsRange>(
        std::move(current), std::move(request), std::move(list_functor),
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
    auto const current = google::cloud::internal::SaveCurrentOptions();
    google::pubsub::v1::DeleteTopicRequest request;
    request.set_topic(p.topic.FullName());
    return RetryLoop(
        retry_policy(*current), backoff_policy(*current),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context, Options const& options,
               google::pubsub::v1::DeleteTopicRequest const& request) {
          return stub_->DeleteTopic(context, options, request);
        },
        *current, request, __func__);
  }

  StatusOr<google::pubsub::v1::DetachSubscriptionResponse> DetachSubscription(
      DetachSubscriptionParams p) override {
    auto const current = google::cloud::internal::SaveCurrentOptions();
    google::pubsub::v1::DetachSubscriptionRequest request;
    request.set_subscription(p.subscription.FullName());
    grpc::ClientContext context;
    return RetryLoop(
        retry_policy(*current), backoff_policy(*current),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context, Options const& options,
               google::pubsub::v1::DetachSubscriptionRequest const& request) {
          return stub_->DetachSubscription(context, options, request);
        },
        *current, request, __func__);
  }

  pubsub::ListTopicSubscriptionsRange ListTopicSubscriptions(
      ListTopicSubscriptionsParams p) override {
    auto current = google::cloud::internal::SaveCurrentOptions();
    google::pubsub::v1::ListTopicSubscriptionsRequest request;
    request.set_topic(std::move(p.topic_full_name));
    // Because we do not have C++14 generalized lambda captures we cannot just
    // use the unique_ptr<> here, so convert to shared_ptr<> instead.
    auto retry = std::shared_ptr<google::cloud::RetryPolicy const>(
        retry_policy(*current));
    auto backoff =
        std::shared_ptr<pubsub::BackoffPolicy const>(backoff_policy(*current));
    char const* function_name = __func__;
    auto list_functor =
        [stub = stub_, retry = std::move(retry), backoff = std::move(backoff),
         function_name](
            Options const& options,
            google::pubsub::v1::ListTopicSubscriptionsRequest const& request) {
          return RetryLoop(
              retry->clone(), backoff->clone(), Idempotency::kIdempotent,
              [stub](
                  grpc::ClientContext& c, Options const& o,
                  google::pubsub::v1::ListTopicSubscriptionsRequest const& r) {
                return stub->ListTopicSubscriptions(c, o, r);
              },
              options, request, function_name);
        };
    return internal::MakePaginationRange<pubsub::ListTopicSubscriptionsRange>(
        std::move(current), std::move(request), std::move(list_functor),
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
    auto current = google::cloud::internal::SaveCurrentOptions();
    google::pubsub::v1::ListTopicSnapshotsRequest request;
    request.set_topic(std::move(p.topic_full_name));
    // The lambda must be copyable to use in MakePaginationRange<>(). We need
    // to wrap the policies in shared pointers.
    auto retry = std::shared_ptr<google::cloud::RetryPolicy const>(
        retry_policy(*current));
    auto backoff =
        std::shared_ptr<pubsub::BackoffPolicy const>(backoff_policy(*current));
    char const* function_name = __func__;
    auto list_functor =
        [stub = stub_, retry = std::move(retry), backoff = std::move(backoff),
         function_name](
            Options const& options,
            google::pubsub::v1::ListTopicSnapshotsRequest const& request) {
          return RetryLoop(
              retry->clone(), backoff->clone(), Idempotency::kIdempotent,
              [stub](grpc::ClientContext& c, Options const& o,
                     google::pubsub::v1::ListTopicSnapshotsRequest const& r) {
                return stub->ListTopicSnapshots(c, o, r);
              },
              options, request, function_name);
        };
    return internal::MakePaginationRange<pubsub::ListTopicSnapshotsRange>(
        std::move(current), std::move(request), std::move(list_functor),
        [](google::pubsub::v1::ListTopicSnapshotsResponse response) {
          std::vector<std::string> items;
          items.reserve(response.snapshots_size());
          for (auto& item : *response.mutable_snapshots()) {
            items.push_back(std::move(item));
          }
          return items;
        });
  }

  Options options() const override { return options_; }

 private:
  std::unique_ptr<google::cloud::BackgroundThreads> background_;
  std::shared_ptr<pubsub_internal::PublisherStub> stub_;
  Options options_;
};

}  // namespace

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
  return internal::MakeUnimplementedPaginationRange<ListTopicsRange>();
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
  return internal::MakeUnimplementedPaginationRange<
      ListTopicSubscriptionsRange>();
}

ListTopicSnapshotsRange TopicAdminConnection::ListTopicSnapshots(
    ListTopicSnapshotsParams) {  // NOLINT(performance-unnecessary-value-param)
  return internal::MakeUnimplementedPaginationRange<ListTopicSnapshotsRange>();
}

std::shared_ptr<TopicAdminConnection> MakeTopicAdminConnection(
    std::initializer_list<internal::NonConstructible>) {
  return MakeTopicAdminConnection();
}

std::shared_ptr<TopicAdminConnection> MakeTopicAdminConnection(Options opts) {
  internal::CheckExpectedOptions<CommonOptionList, GrpcOptionList,
                                 UnifiedCredentialsOptionList,
                                 PolicyOptionList>(opts, __func__);
  opts = pubsub_internal::DefaultCommonOptions(std::move(opts));

  auto background = internal::MakeBackgroundThreadsFactory(opts)();
  auto stub =
      pubsub_internal::MakeRoundRobinPublisherStub(background->cq(), opts);
  return std::make_shared<TopicAdminConnectionImpl>(
      std::move(background), std::move(stub), std::move(opts));
}

std::shared_ptr<TopicAdminConnection> MakeTopicAdminConnection(
    ConnectionOptions const& options,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy) {
  auto opts = internal::MakeOptions(options);
  if (retry_policy) opts.set<RetryPolicyOption>(retry_policy->clone());
  if (backoff_policy) opts.set<BackoffPolicyOption>(backoff_policy->clone());
  return MakeTopicAdminConnection(std::move(opts));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub

namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::shared_ptr<pubsub::TopicAdminConnection> MakeTestTopicAdminConnection(
    Options const& opts, std::shared_ptr<PublisherStub> stub) {
  auto background = internal::MakeBackgroundThreadsFactory(opts)();
  stub = pubsub_internal::MakeTestPublisherStub(background->cq(), opts,
                                                {std::move(stub)});
  return std::make_shared<pubsub::TopicAdminConnectionImpl>(
      std::move(background), std::move(stub), opts);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
