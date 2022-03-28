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

#include "google/cloud/pubsublite/internal/multipartition_publisher.h"
#include <functional>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

using google::cloud::pubsublite::MessageMetadata;
using google::cloud::pubsublite::Topic;
using google::cloud::pubsublite::v1::Cursor;
using google::cloud::pubsublite::v1::PubSubMessage;

MultipartitionPublisher::MultipartitionPublisher(
    std::function<std::unique_ptr<PartitionPublisher>(Partition)>
        publisher_factory,
    std::unique_ptr<TopicPartitionCountReader> reader,
    AlarmRegistry& alarm_registry,
    std::unique_ptr<RoutingPolicy> routing_policy, Topic topic)
    : publisher_factory_{std::move(publisher_factory)},
      reader_{std::move(reader)},
      routing_policy_{std::move(routing_policy)},
      topic_{std::move(topic)},
      cancel_token_{alarm_registry.RegisterAlarm(
          std::chrono::milliseconds{60 * 1000},
          std::bind(&MultipartitionPublisher::CreatePublishers, this))} {}

MultipartitionPublisher::~MultipartitionPublisher() {
  future<void> shutdown = Shutdown();
  if (!shutdown.is_ready()) {
    GCP_LOG(WARNING) << "`Shutdown` must be called and finished before object "
                        "goes out of scope.";
    assert(false);
  }
  shutdown.get();
}

future<Status> MultipartitionPublisher::Start() {
  return service_composite_.Start();
}

void MultipartitionPublisher::CreatePublishers() {
  future<std::uint64_t> read_future;
  {
    std::lock_guard<std::mutex> g{mu_};
    read_future = reader_->Read(topic_);
  }
  read_future.then([this](future<std::uint64_t> num_partitions_future) {
    std::uint64_t num_partitions = num_partitions_future.get();
    std::lock_guard<std::mutex> g{mu_};
    while (partition_publishers_.size() < num_partitions) {
      partition_publishers_.push_back(
          publisher_factory_(partition_publishers_.size()));
      service_composite_.AddServiceObject(partition_publishers_.back().get());
    }
  });
}

future<StatusOr<MessageMetadata>> MultipartitionPublisher::Publish(
    PubSubMessage m) {
  std::lock_guard<std::mutex> g{mu_};
  std::int64_t partition =
      m.key().empty()
          ? routing_policy_->Route(partition_publishers_.size())
          : routing_policy_->Route(m.key(), partition_publishers_.size());
  // TODO(18suresha) handle invalid partition?
  return partition_publishers_[partition]->Publish(m).then(
      [partition](future<StatusOr<Cursor>> publish_future) {
        auto publish_response = publish_future.get();
        if (!publish_response) {
          return StatusOr<MessageMetadata>{publish_response.status()};
        }
        return StatusOr<MessageMetadata>{
            MessageMetadata{partition, std::move(*publish_response)}};
      });
}

void MultipartitionPublisher::Flush() {
  std::lock_guard<std::mutex> g{mu_};
  for (auto& partition_publisher : partition_publishers_) {
    partition_publisher->Flush();
  }
}

future<void> MultipartitionPublisher::Shutdown() {
  cancel_token_ = nullptr;
  return service_composite_.Shutdown();
}

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
