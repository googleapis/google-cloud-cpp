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

#include "google/cloud/pubsub/publisher_connection.h"
#include "google/cloud/pubsub/internal/default_retry_policies.h"
#include "google/cloud/pubsub/internal/publisher_logging.h"
#include "google/cloud/pubsub/internal/publisher_metadata.h"
#include "google/cloud/pubsub/internal/publisher_stub.h"
#include "google/cloud/future_void.h"
#include "google/cloud/internal/async_retry_loop.h"
#include "google/cloud/log.h"
#include <memory>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

PublisherConnection::~PublisherConnection() = default;

std::shared_ptr<PublisherConnection> MakePublisherConnection(
    ConnectionOptions connection_options,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy) {
  auto stub = pubsub_internal::CreateDefaultPublisherStub(connection_options,
                                                          /*channel_id=*/0);
  return pubsub_internal::MakePublisherConnection(
      std::move(connection_options), std::move(stub), std::move(retry_policy),
      std::move(backoff_policy));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub

namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {
class ContainingPublisherConnection : public pubsub::PublisherConnection {
 public:
  ContainingPublisherConnection(std::shared_ptr<BackgroundThreads> background,
                                std::shared_ptr<PublisherConnection> child)
      : background_(std::move(background)), child_(std::move(child)) {}

  ~ContainingPublisherConnection() override = default;

  future<StatusOr<google::pubsub::v1::PublishResponse>> Publish(
      PublishParams p) override {
    return child_->Publish(std::move(p));
  }

  google::cloud::CompletionQueue cq() override { return child_->cq(); }

 private:
  std::shared_ptr<BackgroundThreads> background_;
  std::shared_ptr<PublisherConnection> child_;
};

class PublisherConnectionImpl : public pubsub::PublisherConnection {
 public:
  PublisherConnectionImpl(
      std::shared_ptr<pubsub_internal::PublisherStub> stub,
      google::cloud::CompletionQueue cq,
      std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
      std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy)
      : stub_(std::move(stub)),
        cq_(std::move(cq)),
        retry_policy_(std::move(retry_policy)),
        backoff_policy_(std::move(backoff_policy)) {}

  ~PublisherConnectionImpl() override = default;

  future<StatusOr<google::pubsub::v1::PublishResponse>> Publish(
      PublishParams p) override {
    auto& stub = stub_;
    return google::cloud::internal::AsyncRetryLoop(
        retry_policy_->clone(), backoff_policy_->clone(),
        google::cloud::internal::Idempotency::kIdempotent, cq_,
        [stub](google::cloud::CompletionQueue& cq,
               std::unique_ptr<grpc::ClientContext> context,
               google::pubsub::v1::PublishRequest const& request) {
          return stub->AsyncPublish(cq, std::move(context), request);
        },
        std::move(p.request), __func__);
  }

  google::cloud::CompletionQueue cq() override { return cq_; }

 private:
  std::shared_ptr<pubsub_internal::PublisherStub> stub_;
  google::cloud::CompletionQueue cq_;
  std::unique_ptr<pubsub::RetryPolicy const> retry_policy_;
  std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy_;
};

}  // namespace

std::shared_ptr<pubsub::PublisherConnection> MakePublisherConnection(
    pubsub::ConnectionOptions connection_options,
    std::shared_ptr<PublisherStub> stub,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy) {
  if (!retry_policy) retry_policy = DefaultRetryPolicy();
  if (!backoff_policy) backoff_policy = DefaultBackoffPolicy();
  stub = std::make_shared<pubsub_internal::PublisherMetadata>(std::move(stub));
  if (connection_options.tracing_enabled("rpc")) {
    GCP_LOG(INFO) << "Enabled logging for gRPC calls";
    stub = std::make_shared<pubsub_internal::PublisherLogging>(
        std::move(stub), connection_options.tracing_options());
  }

  auto default_thread_pool_size = []() -> std::size_t {
    auto constexpr kDefaultThreadPoolSize = 4;
    auto const n = std::thread::hardware_concurrency();
    return n == 0 ? kDefaultThreadPoolSize : n;
  };
  if (connection_options.background_thread_pool_size() == 0) {
    connection_options.set_background_thread_pool_size(
        default_thread_pool_size());
  }
  auto background = connection_options.background_threads_factory()();
  auto connection = std::make_shared<PublisherConnectionImpl>(
      std::move(stub), background->cq(), std::move(retry_policy),
      std::move(backoff_policy));
  return std::make_shared<ContainingPublisherConnection>(std::move(background),
                                                         std::move(connection));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
