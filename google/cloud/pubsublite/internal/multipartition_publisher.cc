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

future<StatusOr<std::uint32_t>> MultipartitionPublisher::GetNumPartitions() {
  return admin_connection_->AsyncGetTopicPartitions(topic_partitions_request_)
      .then([](future<StatusOr<TopicPartitions>> f) -> StatusOr<std::uint32_t> {
        auto partitions = f.get();
        if (!partitions) return std::move(partitions).status();
        if (partitions->partition_count() >=
            std::numeric_limits<std::uint32_t>::max()) {
          return Status{StatusCode::kInternal,
                        absl::StrCat("Returned partition count is too big: ",
                                     partitions->partition_count())};
        }
        return static_cast<std::uint32_t>(partitions->partition_count());
      });
}

void MultipartitionPublisher::HandleNumPartitions(
    std::uint32_t num_partitions) {
  std::uint32_t current_num_partitions;
  {
    std::lock_guard<std::mutex> g{mu_};
    current_num_partitions =
        static_cast<std::uint32_t>(partition_publishers_.size());
  }
  assert(num_partitions >= current_num_partitions);  // should be no race
  if (num_partitions == current_num_partitions) return;
  std::vector<std::shared_ptr<Publisher<Cursor>>> new_partition_publishers;
  for (std::uint32_t partition = current_num_partitions;
       partition < num_partitions; ++partition) {
    new_partition_publishers.push_back(publisher_factory_(partition));
    service_composite_.AddServiceObject(new_partition_publishers.back().get());
  }
  {
    std::lock_guard<std::mutex> g{mu_};
    std::move(new_partition_publishers.begin(), new_partition_publishers.end(),
              std::back_inserter(partition_publishers_));
  }
  TryPublishMessages();
}

void MultipartitionPublisher::TriggerPublisherCreation() {
  {
    std::lock_guard<std::mutex> g{mu_};
    if (outstanding_num_partitions_req_) return;
    outstanding_num_partitions_req_.emplace();
  }
  GetNumPartitions()
      .then([this](future<StatusOr<std::uint32_t>> f) {
        if (!service_composite_.status().ok()) return;
        auto num_partitions = f.get();
        if (num_partitions.ok()) return HandleNumPartitions(*num_partitions);
        GCP_LOG(WARNING) << "Reading number of partitions for "
                         << topic_.FullName()
                         << "failed: " << num_partitions.status();
        bool first_poll;
        {
          std::lock_guard<std::mutex> g{mu_};
          first_poll = partition_publishers_.empty();
        }
        if (first_poll) {
          // fail client if first poll fails
          return service_composite_.Abort(num_partitions.status());
        }
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
  std::uint32_t partition =
      state.message.key().empty()
          ? routing_policy_->Route(state.num_partitions)
          : routing_policy_->Route(state.message.key(), state.num_partitions);
  Publisher<Cursor>* publisher;
  {
    std::lock_guard<std::mutex> g{mu_};
    publisher = partition_publishers_.at(partition).get();
  }

  struct OnPublish {
    promise<StatusOr<MessageMetadata>> p;
    std::uint32_t partition;
    void operator()(future<StatusOr<Cursor>> f) {
      auto publish_response = f.get();
      if (!publish_response) {
        return p.set_value(std::move(publish_response).status());
      }
      p.set_value(MessageMetadata{partition, *std::move(publish_response)});
    }
  };

  publisher->Publish(std::move(state.message))
      .then(OnPublish{std::move(state.publish_promise), partition});
}

void MultipartitionPublisher::TryPublishMessages() {
  {
    std::lock_guard<std::mutex> g{mu_};
    if (in_publish_loop_) return;
    in_publish_loop_ = true;
  }
  while (true) {
    std::deque<PublishState> messages;
    std::uint32_t num_partitions;
    {
      std::lock_guard<std::mutex> g{mu_};
      if (messages_.empty()) {
        in_publish_loop_ = false;
        return;
      }
      messages.swap(messages_);
      num_partitions = static_cast<std::uint32_t>(partition_publishers_.size());
    }
    for (PublishState& state : messages) {
      state.num_partitions = num_partitions;
      RouteAndPublish(std::move(state));
    }
  }
}

future<StatusOr<MessageMetadata>> MultipartitionPublisher::Publish(
    PubSubMessage m) {
  if (!service_composite_.status().ok()) {
    return make_ready_future(
        StatusOr<MessageMetadata>(service_composite_.status()));
  }
  PublishState state;
  state.message = std::move(m);
  future<StatusOr<MessageMetadata>> to_return;
  {
    std::lock_guard<std::mutex> g{mu_};
    to_return = state.publish_promise.get_future();
    messages_.push_back(std::move(state));
    // message will be published whenever we successfully read the number of
    // partitions and publishers are created
    if (partition_publishers_.empty()) return to_return;
  }
  TryPublishMessages();
  return to_return;
}

void MultipartitionPublisher::Flush() {
  std::vector<Publisher<Cursor>*> publishers;
  {
    std::lock_guard<std::mutex> g{mu_};
    publishers.reserve(partition_publishers_.size());
    for (auto& publisher : partition_publishers_) {
      publishers.push_back(publisher.get());
    }
  }
  for (auto* publisher : publishers) {
    publisher->Flush();
  }
}

future<void> MultipartitionPublisher::Shutdown() {
  cancel_token_ = nullptr;
  auto shutdown = service_composite_.Shutdown();

  std::deque<PublishState> messages;
  {
    std::lock_guard<std::mutex> g{mu_};
    messages.swap(messages_);
  }
  for (auto& state : messages) {
    state.publish_promise.set_value(service_composite_.status());
  }

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
