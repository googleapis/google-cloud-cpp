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
      cancel_token_{alarm_registry.RegisterAlarm(
          std::chrono::seconds{60},
          std::bind(&MultipartitionPublisher::CreatePublishers, this))} {
  *topic_partitions_request_.mutable_name() = topic_.FullName();
}

future<Status> MultipartitionPublisher::Start() {
  CreatePublishers();
  return service_composite_.Start();
}

future<StatusOr<std::uint32_t>> MultipartitionPublisher::GetNumPartitions() {
  future<StatusOr<TopicPartitions>> topic_partitions_request;
  topic_partitions_request =
      admin_connection_->AsyncGetTopicPartitions(topic_partitions_request_);
  return topic_partitions_request.then([](future<StatusOr<TopicPartitions>> f) {
    auto partitions = f.get();
    if (!partitions) return StatusOr<std::uint32_t>{partitions.status()};
    if (partitions->partition_count() >= UINT32_MAX) {
      return StatusOr<std::uint32_t>{
          Status{StatusCode::kFailedPrecondition,
                 absl::StrCat("Returned partition count is too big: ",
                              std::to_string(partitions->partition_count()))}};
    }
    return StatusOr<std::uint32_t>{
        static_cast<std::uint32_t>(partitions->partition_count())};
  });
}

void MultipartitionPublisher::CreatePublishers() {
  GetNumPartitions().then([this](future<StatusOr<std::uint32_t>> f) {
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
    bool was_empty = partition_publishers_.empty();
    for (std::uint64_t i = partition_publishers_.size(); i < *num_partitions;
         ++i) {
      new_partition_publishers.push_back(publisher_factory_(i));
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
      if (was_empty) {
        for (auto& state : initial_publish_buffer_) {
          state.num_partitions = *num_partitions;
          initial_publish_buffer.push_back(std::move(state));
        }
        initial_publish_buffer_.clear();
      }
    }
    for (auto& state : initial_publish_buffer) {
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
          message_promise->set_value(
              StatusOr<MessageMetadata>{publish_response.status()});
        }
        return message_promise->set_value(StatusOr<MessageMetadata>{
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
    state.num_partitions =
        static_cast<std::uint32_t>(partition_publishers_.size());
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

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
