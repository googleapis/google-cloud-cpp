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
#include <limits>

namespace google {
namespace cloud {
namespace pubsublite_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::pubsublite::AdminServiceConnection;
using ::google::cloud::pubsublite::MessageMetadata;
using ::google::cloud::pubsublite::Topic;
using ::google::cloud::pubsublite::v1::Cursor;
using ::google::cloud::pubsublite::v1::PubSubMessage;
using ::google::cloud::pubsublite::v1::TopicPartitions;

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
      cancel_token_{alarm_registry.RegisterAlarm(
          std::chrono::seconds{60}, [this]() { TriggerPublisherCreation(); })} {
}

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
  auto start = service_composite_.Start();
  TriggerPublisherCreation();
  return start;
}

future<StatusOr<Partition>> MultipartitionPublisher::GetNumPartitions() {
  return admin_connection_->AsyncGetTopicPartitions(topic_partitions_request_)
      .then([](future<StatusOr<TopicPartitions>> f) -> StatusOr<Partition> {
        auto partitions = f.get();
        if (!partitions) return partitions.status();
        if (partitions->partition_count() >=
            std::numeric_limits<Partition>::max()) {
          return Status{StatusCode::kFailedPrecondition,
                        absl::StrCat("Returned partition count is too big: ",
                                     partitions->partition_count())};
        }
        return static_cast<Partition>(partitions->partition_count());
      });
}

void MultipartitionPublisher::HandleNumPartitions(Partition num_partitions) {
  Partition current_num_partitions;
  {
    std::lock_guard<std::mutex> g{mu_};
    current_num_partitions =
        static_cast<Partition>(partition_publishers_.size());
    if (updating_partitions_ || current_num_partitions >= num_partitions) {
      return;
    }
    updating_partitions_ = true;
  }
  std::vector<std::unique_ptr<Publisher<Cursor>>> new_partition_publishers;
  for (Partition i = current_num_partitions; i < num_partitions; ++i) {
    new_partition_publishers.push_back(
        publisher_factory_(static_cast<Partition>(i)));
    service_composite_.AddServiceObject(new_partition_publishers.back().get());
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
    state.num_partitions = num_partitions;
    RouteAndPublish(std::move(state));
  }
}

void MultipartitionPublisher::TriggerPublisherCreation() {
  {
    std::lock_guard<std::mutex> g{mu_};
    if (outstanding_num_partitions_req_) return;
    outstanding_num_partitions_req_.emplace();
  }
  GetNumPartitions()
      .then([this](future<StatusOr<Partition>> f) {
        if (!service_composite_.status().ok()) return;
        auto num_partitions = f.get();
        if (num_partitions.ok()) return HandleNumPartitions(*num_partitions);
        bool first_poll;
        {
          std::lock_guard<std::mutex> g{mu_};
          first_poll = partition_publishers_.empty();
        }
        if (first_poll) {
          // fail client if first poll fails
          return service_composite_.Abort(num_partitions.status());
        }
        GCP_LOG(WARNING) << "Reading number of partitions for "
                         << topic_.FullName()
                         << "failed: " << num_partitions.status();
      })
      .then([this](future<void>) {
        absl::optional<promise<void>> p;
        {
          std::lock_guard<std::mutex> g{mu_};
          assert(outstanding_num_partitions_req_);
          p.swap(outstanding_num_partitions_req_);
        }
        // only set value after done touching member variables
        p->set_value();
      });
}

void MultipartitionPublisher::RouteAndPublish(PublishState state) {
  Partition partition =
      state.message.key().empty()
          ? routing_policy_->Route(state.num_partitions)
          : routing_policy_->Route(state.message.key(), state.num_partitions);
  Publisher<Cursor>* publisher;
  {
    std::lock_guard<std::mutex> g{mu_};
    publisher = partition_publishers_.at(partition).get();
  }
  auto shared_promise = std::make_shared<promise<StatusOr<MessageMetadata>>>(
      std::move(state.publish_promise));
  publisher->Publish(std::move(state.message))
      .then(
          [partition, shared_promise](future<StatusOr<Cursor>> publish_future) {
            auto publish_response = publish_future.get();
            if (!publish_response) {
              return shared_promise->set_value(publish_response.status());
            }
            shared_promise->set_value(
                MessageMetadata{partition, std::move(*publish_response)});
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
      return initial_publish_buffer_.back().publish_promise.get_future();
    }
    state.num_partitions = static_cast<Partition>(partition_publishers_.size());
  }

  auto to_return = state.publish_promise.get_future();
  RouteAndPublish(std::move(state));
  return to_return;
}

void MultipartitionPublisher::Flush() {
  std::lock_guard<std::mutex> g{mu_};
  for (auto& partition_publisher : partition_publishers_) {
    partition_publisher->Flush();
  }
}

future<void> MultipartitionPublisher::Shutdown() {
  cancel_token_ = nullptr;
  std::vector<PublishState> initial_publish_buffer;
  {
    std::lock_guard<std::mutex> g{mu_};
    initial_publish_buffer.swap(initial_publish_buffer_);
  }
  for (auto& state : initial_publish_buffer) {
    state.publish_promise.set_value(Status{
        StatusCode::kFailedPrecondition, "Multipartition publisher shutdown"});
  }
  // intentionally invoke shutdown state first
  auto shutdown = service_composite_.Shutdown();
  std::lock_guard<std::mutex> g{mu_};
  if (outstanding_num_partitions_req_) {
    return outstanding_num_partitions_req_->get_future().then(
        ChainFuture(std::move(shutdown)));
  }
  return shutdown;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google
