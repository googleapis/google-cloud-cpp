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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_DEFAULT_BATCH_SINK_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_DEFAULT_BATCH_SINK_H

#include "google/cloud/pubsub/backoff_policy.h"
#include "google/cloud/pubsub/internal/batch_sink.h"
#include "google/cloud/pubsub/internal/publisher_stub.h"
#include "google/cloud/pubsub/retry_policy.h"
#include "google/cloud/pubsub/version.h"
#include <memory>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/// Publish message batches using a stub, with retries, but no queueing.
class DefaultBatchSink : public BatchSink {
 public:
  static std::shared_ptr<DefaultBatchSink> Create(
      std::shared_ptr<pubsub_internal::PublisherStub> stub,
      google::cloud::CompletionQueue cq,
      std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
      std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy) {
    return std::shared_ptr<DefaultBatchSink>(new DefaultBatchSink(
        std::move(stub), std::move(cq), std::move(retry_policy),
        std::move(backoff_policy)));
  }

  ~DefaultBatchSink() override = default;

  future<StatusOr<google::pubsub::v1::PublishResponse>> AsyncPublish(
      google::pubsub::v1::PublishRequest request) override;

  void ResumePublish(std::string const& ordering_key) override;

 private:
  DefaultBatchSink(std::shared_ptr<pubsub_internal::PublisherStub> stub,
                   google::cloud::CompletionQueue cq,
                   std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
                   std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy);

  std::shared_ptr<pubsub_internal::PublisherStub> stub_;
  google::cloud::CompletionQueue cq_;
  std::unique_ptr<pubsub::RetryPolicy const> retry_policy_;
  std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_DEFAULT_BATCH_SINK_H
