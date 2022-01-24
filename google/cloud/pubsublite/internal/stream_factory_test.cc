// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/pubsublite/internal/stream_factory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsublite_internal {

TEST(StreamFactoryTest, CreateStreams) {
  CompletionQueue queue;
  ClientMetadata metadata{{"key1", "value1"}, {"key2", "value2"}};
  auto publish_factory = MakeStreamFactory(
      std::shared_ptr<PublisherServiceStub>(nullptr), queue, metadata);
  auto subscribe_factory = MakeStreamFactory(
      std::shared_ptr<SubscriberServiceStub>(nullptr), queue, metadata);
  auto cursor_factory = MakeStreamFactory(
      std::shared_ptr<CursorServiceStub>(nullptr), queue, metadata);
  auto assignment_factory = MakeStreamFactory(
      std::shared_ptr<PartitionAssignmentServiceStub>(nullptr), queue,
      metadata);
}

}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google
