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
          std::chrono::seconds{60},
          std::bind(&MultipartitionPublisher::CreatePublishers, this))} {}

MultipartitionPublisher::~MultipartitionPublisher() {
  {
    std::lock_guard<std::mutex> g{mu_};
    if (cancel_token_ != nullptr) cancel_token_ = nullptr;
  }
  future<void> shutdown = Shutdown();
  if (!shutdown.is_ready()) {
    GCP_LOG(WARNING) << "`Shutdown` must be called and finished before object "
                        "goes out of scope.";
    assert(false);
  }
  shutdown.get();
}

future<Status> MultipartitionPublisher::Start() {
  CreatePublishers();
  return service_composite_.Start();
}

void MultipartitionPublisher::CreatePublishers() {
  future<StatusOr<std::uint32_t>> read_future;
  {
    std::lock_guard<std::mutex> g{mu_};
    read_future = reader_->Read(topic_);
  }
  read_future.then([this](future<StatusOr<std::uint32_t>> f) {
    auto num_partitions = f.get();
    if (!num_partitions.ok()) {
      GCP_LOG(WARNING) << "Reading number of partitions for "
                       << topic_.FullName()
                       << "failed: " << num_partitions.status();
      return;
    }
    {
      std::lock_guard<std::mutex> g{mu_};
      if (updating_partitions_ ||
          partition_publishers_.size() == *num_partitions) {
        return;
      }
      updating_partitions_ = true;
    }
    std::vector<std::unique_ptr<PartitionPublisher>> new_partition_publishers;
    for (std::uint64_t i = partition_publishers_.size(); i < *num_partitions;
         ++i) {
      new_partition_publishers.push_back(publisher_factory_(i));
      service_composite_.AddServiceObject(
          new_partition_publishers.back().get());
    }
    std::lock_guard<std::mutex> g{mu_};
    for (auto& partition_publisher : new_partition_publishers) {
      partition_publishers_.push_back(std::move(partition_publisher));
    }
    updating_partitions_ = false;
  });
}

future<StatusOr<MessageMetadata>> MultipartitionPublisher::Publish(
    PubSubMessage m) {
  std::lock_guard<std::mutex> g{mu_};
  if (partition_publishers_.empty()) {
    return make_ready_future(StatusOr<MessageMetadata>{
        Status{StatusCode::kFailedPrecondition,
               "Publishers haven't been created yet."}});
  }
  std::int64_t partition =
      m.key().empty()
          ? routing_policy_->Route(
                static_cast<std::uint32_t>(partition_publishers_.size()))
          : routing_policy_->Route(m.key(), static_cast<std::uint32_t>(
                                                partition_publishers_.size()));
  // TODO(18suresha) handle invalid partition?
  return partition_publishers_.at(partition)->Publish(m).then(
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
