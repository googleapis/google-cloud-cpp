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

#include "google/cloud/pubsublite/internal/topic_partition_count_reader_impl.h"
#include <google/cloud/pubsublite/v1/admin.pb.h>
#include <thread>
#include <utility>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

using google::cloud::pubsublite::AdminServiceConnection;
using google::cloud::pubsublite::v1::GetTopicPartitionsRequest;
using google::cloud::pubsublite::v1::TopicPartitions;

TopicPartitionCountReaderImpl::TopicPartitionCountReaderImpl(
    std::shared_ptr<AdminServiceConnection>
        connection)
    : connection_{std::move(connection)} {};

future<StatusOr<std::uint64_t>> TopicPartitionCountReaderImpl::Read(
    google::cloud::pubsublite::Topic topic) {
  GetTopicPartitionsRequest req;
  *req.mutable_name() = topic.FullName();
  auto p = std::make_shared<promise<StatusOr<TopicPartitions>>>();
  auto connection = connection_;
  // creating stack-based variables and copying to avoid lifetime issues with `this`
  std::thread t{[connection, p, topic, req]() {
    auto partition_count = connection->GetTopicPartitions(req);
    p->set_value(std::move(partition_count));
  }};
  t.detach();
  return p->get_future().then([](future<StatusOr<TopicPartitions>> f) {
    auto partitions = f.get();
    if (!partitions.ok()) return StatusOr<std::uint64_t>{partitions.status()};
    return StatusOr<std::uint64_t>{partitions.value().partition_count()};
  });
}

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
