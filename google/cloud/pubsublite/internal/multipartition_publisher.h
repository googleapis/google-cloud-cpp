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

#include "google/cloud/pubsublite/batching_options.h"
#include "google/cloud/pubsublite/internal/alarm_registry.h"
#include "google/cloud/pubsublite/internal/partition_publisher.h"
#include "google/cloud/pubsublite/internal/routing_policy.h"
#include "google/cloud/pubsublite/internal/service_composite.h"
#include "google/cloud/pubsublite/internal/topic_partition_count_reader.h"
#include "google/cloud/pubsublite/message_metadata.h"
#include "google/cloud/pubsublite/topic_path.h"
#include "absl/functional/function_ref.h"
#include <google/cloud/pubsublite/v1/publisher.pb.h>
#include <mutex>
#include <vector>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

class MultipartitionPublisher
    : public Publisher<google::cloud::pubsublite::MessageMetadata> {
 public:
  MultipartitionPublisher(
      std::function<std::unique_ptr<PartitionPublisher>(std::int64_t)>,
      std::unique_ptr<TopicPartitionCountReader>, AlarmRegistry&,
      std::unique_ptr<RoutingPolicy>, google::cloud::pubsublite::TopicPath);

  ~MultipartitionPublisher() override;

  future<Status> Start() override;

  future<StatusOr<google::cloud::pubsublite::MessageMetadata>> Publish(
      google::cloud::pubsublite::v1::PubSubMessage m) override;

  void Flush() override;

  future<void> Shutdown() override;

 private:
  void CreatePublishers();

  using Partition = std::int64_t;

  std::function<std::unique_ptr<PartitionPublisher>(Partition)>
      publisher_factory_;

  std::mutex mu_;

  std::vector<std::unique_ptr<PartitionPublisher>>
      partition_publishers_;                           // ABSL_GUARDED_BY(mu_)
  std::unique_ptr<TopicPartitionCountReader> reader_;  // ABSL_GUARDED_BY(mu_)
  ServiceComposite service_composite_;
  std::unique_ptr<RoutingPolicy> routing_policy_;  // ABSL_GUARDED_BY(mu_)
  google::cloud::pubsublite::TopicPath topic_path_;
  std::unique_ptr<AlarmRegistry::CancelToken> cancel_token_;
};

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_MULTIPARTITION_PUBLISHER_H
