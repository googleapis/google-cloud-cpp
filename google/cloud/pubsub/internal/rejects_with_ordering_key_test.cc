// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/pubsub/internal/rejects_with_ordering_key.h"
#include "google/cloud/pubsub/mocks/mock_publisher_connection.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::testing::HasSubstr;

TEST(RejectsWithOrderingKeyTest, PublishWithOrderingKeyFailure) {
  auto mock = std::make_shared<pubsub_mocks::MockPublisherConnection>();
  pubsub::Topic const topic("test-project", "test-topic");

  auto publisher = RejectsWithOrderingKey::Create(mock);
  auto response =
      publisher
          ->Publish({pubsub::MessageBuilder{}
                         .SetData("test-data-0")
                         .SetOrderingKey("test-ordering-key-0")
                         .Build()})
          .then([&](future<StatusOr<std::string>> f) {
            auto response = f.get();
            EXPECT_EQ(StatusCode::kInvalidArgument, response.status().code());
            EXPECT_THAT(response.status().message(),
                        HasSubstr("does not have message ordering enabled"));
          });
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
