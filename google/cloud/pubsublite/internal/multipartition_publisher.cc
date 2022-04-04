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
namespace pubsublite_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using google::cloud::pubsublite::AdminServiceConnection;
using google::cloud::pubsublite::MessageMetadata;
using google::cloud::pubsublite::Topic;
using google::cloud::pubsublite::v1::Cursor;
using google::cloud::pubsublite::v1::PubSubMessage;
using google::cloud::pubsublite::v1::TopicPartitions;

MultipartitionPublisher::MultipartitionPublisher(
    PartitionPublisherFactory publisher_factory,
    std::shared_ptr<AdminServiceConnection> admin_connection,
    AlarmRegistry& alarm_registry,
    std::unique_ptr<RoutingPolicy> routing_policy, Topic topic)
    : publisher_factory_{std::move(publisher_factory)},
      admin_connection_{std::move(admin_connection)},
      routing_policy_{std::move(routing_policy)},
      topic_{std::move(topic)},
      topic_partitions_request_{[&] {
        google::cloud::pubsublite::v1::GetTopicPartitionsRequest request;
        *request.mutable_name() = topic_.FullName();
        return request;
      }()},
      cancel_token_{
          alarm_registry.RegisterAlarm(std::chrono::seconds{60}, [this]() {
            if (!service_composite_.status().ok()) return;
            TriggerPublisherCreation();
          })} {}

future<Status> MultipartitionPublisher::Start() {
  TriggerPublisherCreation();
  return service_composite_.Start();
}

future<StatusOr<std::uint32_t>> MultipartitionPublisher::GetNumPartitions() {
  return admin_connection_->AsyncGetTopicPartitions(topic_partitions_request_)
      .then([](future<StatusOr<TopicPartitions>> f) -> StatusOr<Partition> {
        auto partitions = f.get();
        if (!partitions) return partitions.status();
        if (partitions->partition_count() >= UINT32_MAX) {
          return Status{
              StatusCode::kFailedPrecondition,
              absl::StrCat("Returned partition count is too big: ",
                           std::to_string(partitions->partition_count()))};
        }
        return static_cast<Partition>(partitions->partition_count());
      });
}

void MultipartitionPublisher::TriggerPublisherCreation() {
  GetNumPartitions().then([this](future<StatusOr<Partition>> f) {
    auto num_partitions = f.get();
    if (!num_partitions.ok()) {
      GCP_LOG(WARNING) << "Reading number of partitions for "
                       << topic_.FullName()
                       << "failed: " << num_partitions.status();
      return;
    }
    std::uint64_t current_num_partitions;
    {
      std::lock_guard<std::mutex> g{mu_};
      if (updating_partitions_ ||
          partition_publishers_.size() == *num_partitions) {
        return;
      }
      current_num_partitions = partition_publishers_.size();
      updating_partitions_ = true;
    }
    std::vector<std::unique_ptr<Publisher<Cursor>>> new_partition_publishers;
    for (std::uint64_t i = current_num_partitions; i < *num_partitions; ++i) {
      new_partition_publishers.push_back(
          publisher_factory_(static_cast<Partition>(i)));
      service_composite_.AddServiceObject(
          new_partition_publishers.back().get());
    }
    std::vector<PublishState> initial_publish_buffer;
    {
      std::lock_guard<std::mutex> g{mu_};
      for (auto& partition_publisher : new_partition_publishers) {
        partition_publishers_.push_back(std::move(partition_publisher));
      }
      updating_partitions_ = false;
      initial_publish_buffer.swap(initial_publish_buffer_);
    }
    for (auto& state : initial_publish_buffer) {
      state.num_partitions = *num_partitions;
      RouteAndPublish(state);
    }
  });
}

void MultipartitionPublisher::RouteAndPublish(PublishState& state) {
  std::int64_t partition =
      state.message.key().empty()
          ? routing_policy_->Route(state.num_partitions)
          : routing_policy_->Route(state.message.key(), state.num_partitions);
  Publisher<Cursor>* publisher;
  {
    std::lock_guard<std::mutex> g{mu_};
    publisher = partition_publishers_.at(partition).get();
  }
  auto message_promise = state.publish_promise;
  publisher->Publish(std::move(state.message))
      .then([partition,
             message_promise](future<StatusOr<Cursor>> publish_future) {
        auto publish_response = publish_future.get();
        if (!publish_response) {
          return message_promise->set_value(
              StatusOr<MessageMetadata>{publish_response.status()});
        }
        message_promise->set_value(StatusOr<MessageMetadata>{
            MessageMetadata{partition, std::move(*publish_response)}});
      });
}

future<StatusOr<MessageMetadata>> MultipartitionPublisher::Publish(
    PubSubMessage m) {
  PublishState state;
  state.message = std::move(m);
  {
    std::lock_guard<std::mutex> g{mu_};
    // performing this under lock to guarantee that all of
    // `initial_publish_buffer_` is flushed when publishers are created
    if (partition_publishers_.empty()) {
      initial_publish_buffer_.push_back(std::move(state));
      return initial_publish_buffer_.back().publish_promise->get_future();
    }
    state.num_partitions = static_cast<Partition>(partition_publishers_.size());
  }

  RouteAndPublish(state);
  return state.publish_promise->get_future();
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

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google
