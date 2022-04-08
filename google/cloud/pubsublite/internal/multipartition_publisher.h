// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_MULTIPARTITION_PUBLISHER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_MULTIPARTITION_PUBLISHER_H

#include "google/cloud/pubsublite/admin_connection.h"
#include "google/cloud/pubsublite/internal/alarm_registry.h"
#include "google/cloud/pubsublite/internal/partition_publisher.h"
#include "google/cloud/pubsublite/internal/routing_policy.h"
#include "google/cloud/pubsublite/internal/service_composite.h"
#include "google/cloud/pubsublite/message_metadata.h"
#include "google/cloud/pubsublite/topic.h"
#include <google/cloud/pubsublite/v1/admin.pb.h>
#include <google/cloud/pubsublite/v1/publisher.pb.h>
#include <mutex>
#include <vector>

namespace google {
namespace cloud {
namespace pubsublite_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class MultipartitionPublisher
    : public Publisher<google::cloud::pubsublite::MessageMetadata> {
 private:
  // This returns a uniquely owned Publisher, but is a shared_ptr for testing
  // purposes.
  using PartitionPublisherFactory = std::function<std::shared_ptr<
      Publisher<google::cloud::pubsublite::v1::Cursor>>(Partition)>;

 public:
  MultipartitionPublisher(
      PartitionPublisherFactory publisher_factory,
      std::shared_ptr<google::cloud::pubsublite::AdminServiceConnection>
          admin_connection,
      AlarmRegistry& alarm_registry,
      std::unique_ptr<RoutingPolicy> routing_policy,
      google::cloud::pubsublite::Topic topic);

  ~MultipartitionPublisher() override;

  future<Status> Start() override;

  future<StatusOr<google::cloud::pubsublite::MessageMetadata>> Publish(
      google::cloud::pubsublite::v1::PubSubMessage m) override;

  void Flush() override;

  future<void> Shutdown() override;

 private:
  struct PublishState {
    std::uint32_t num_partitions;
    google::cloud::pubsublite::v1::PubSubMessage message;
    promise<StatusOr<google::cloud::pubsublite::MessageMetadata>>
        publish_promise;
  };

  void TriggerPublisherCreation();

  future<StatusOr<std::uint32_t>> GetNumPartitions();

  void RouteAndPublish(PublishState state);

  void HandleNumPartitions(std::uint32_t num_partitions);

  PartitionPublisherFactory publisher_factory_;

  std::mutex mu_;

  std::vector<std::shared_ptr<Publisher<google::cloud::pubsublite::v1::Cursor>>>
      partition_publishers_;  // ABSL_GUARDED_BY(mu_)
  // stores messages intended to be `Publish`ed when there were no partition
  // publishers available
  // this buffer will be cleared and messages will be sent when the first
  // partition publisher becomes available
  std::vector<PublishState> initial_publish_buffer_;  // ABSL_GUARDED_BY(mu_)
  absl::optional<promise<void>>
      outstanding_num_partitions_req_;  // ABSL_GUARDED_BY(mu_)
  std::shared_ptr<google::cloud::pubsublite::AdminServiceConnection> const
      admin_connection_;
  ServiceComposite service_composite_;
  std::unique_ptr<RoutingPolicy> const routing_policy_;
  google::cloud::pubsublite::Topic const topic_;
  google::cloud::pubsublite::v1::GetTopicPartitionsRequest const
      topic_partitions_request_;
  std::unique_ptr<AlarmRegistry::CancelToken> cancel_token_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_MULTIPARTITION_PUBLISHER_H
