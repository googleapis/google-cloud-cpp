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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_TOPIC_PARTITION_COUNT_READER_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_TOPIC_PARTITION_COUNT_READER_IMPL_H

#include "google/cloud/pubsublite/admin_connection.h"
#include "google/cloud/pubsublite/internal/topic_partition_count_reader.h"
#include "google/cloud/pubsublite/topic.h"
#include "google/cloud/future.h"
#include "google/cloud/version.h"
#include <memory>
#include <utility>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

class TopicPartitionCountReaderImpl : public TopicPartitionCountReader {
 public:
  explicit TopicPartitionCountReaderImpl(
      std::shared_ptr<google::cloud::pubsublite::AdminServiceConnection>
          connection);
  future<StatusOr<std::uint32_t>> Read(
      google::cloud::pubsublite::Topic topic) override;

 private:
  std::shared_ptr<google::cloud::pubsublite::AdminServiceConnection>
      connection_;
};

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_TOPIC_PARTITION_COUNT_READER_IMPL_H
