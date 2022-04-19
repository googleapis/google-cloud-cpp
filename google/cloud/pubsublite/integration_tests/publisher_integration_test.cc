// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/pubsublite/message_metadata.h"
#include "google/cloud/pubsublite/options.h"
#include "google/cloud/pubsublite/publisher_connection.h"
#include "google/cloud/version.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsublite {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::MockFunction;
using ::testing::StrictMock;

using google::cloud::pubsub::MessageBuilder;

unsigned int constexpr kNumMessages = 500;

TEST(PublisherIntegrationTest, BasicGoodWithoutKey) {
  auto publisher =
      *MakePublisherConnection(Topic{"<good>", "<good>", "<good>"}, Options{});
  std::vector<future<StatusOr<std::string>>> results;
  for (unsigned int i = 0; i != kNumMessages; ++i) {
    results.push_back(publisher->Publish(
        {MessageBuilder{}.SetData("abcde-" + std::to_string(i)).Build()}));
  }
  for (unsigned i = 0; i != kNumMessages; ++i) {
    EXPECT_TRUE(results[i].get());
  }
}

TEST(PublisherIntegrationTest, BasicGoodWithKey) {
  auto publisher =
      *MakePublisherConnection(Topic{"<good>", "<good>", "<good>"}, Options{});
  std::vector<future<StatusOr<std::string>>> results;
  for (unsigned int i = 0; i != kNumMessages; ++i) {
    results.push_back(
        publisher->Publish({MessageBuilder{}
                                .SetData("abcde-" + std::to_string(i))
                                .SetOrderingKey("key")
                                .Build()}));
  }
  for (unsigned i = 0; i != kNumMessages; ++i) {
    EXPECT_TRUE(results[i].get());
  }
}

TEST(PublisherIntegrationTest, InvalidTopic) {
  StrictMock<MockFunction<void(Status)>> failure_handler;
  auto publisher = *MakePublisherConnection(
      Topic{"123456", "us-up1-b", "tpc"},
      Options{}.set<FailureHandlerOption>(failure_handler.AsStdFunction()));
  EXPECT_CALL(failure_handler, Call);
  EXPECT_EQ(publisher->Publish({MessageBuilder{}.Build()})
                .wait_for(std::chrono::seconds(2)),
            std::future_status::timeout);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite
}  // namespace cloud
}  // namespace google
