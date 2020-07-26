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

#include "google/cloud/pubsub/internal/default_ack_handler_impl.h"
#include "google/cloud/pubsub/testing/mock_subscriber_stub.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::testing::_;

TEST(DefaultAckHandlerTest, Ack) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, AsyncAcknowledge(_, _, _))
      .WillOnce([](google::cloud::CompletionQueue&,
                   std::unique_ptr<grpc::ClientContext>,
                   google::pubsub::v1::AcknowledgeRequest const& request) {
        EXPECT_EQ("test-subscription", request.subscription());
        EXPECT_EQ(1, request.ack_ids_size());
        EXPECT_EQ("test-ack-id", request.ack_ids(0));
        return make_ready_future(Status{});
      });

  google::cloud::CompletionQueue cq;
  DefaultAckHandlerImpl handler(cq, mock, "test-subscription", "test-ack-id");
  EXPECT_EQ("test-ack-id", handler.ack_id());
  ASSERT_NO_FATAL_FAILURE(handler.ack());
}

TEST(DefaultAckHandlerTest, Nack) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, AsyncModifyAckDeadline(_, _, _))
      .WillOnce(
          [](google::cloud::CompletionQueue&,
             std::unique_ptr<grpc::ClientContext>,
             google::pubsub::v1::ModifyAckDeadlineRequest const& request) {
            EXPECT_EQ("test-subscription", request.subscription());
            EXPECT_EQ(1, request.ack_ids_size());
            EXPECT_EQ("test-ack-id", request.ack_ids(0));
            EXPECT_EQ(0, request.ack_deadline_seconds());
            return make_ready_future(Status{});
          });

  google::cloud::CompletionQueue cq;
  DefaultAckHandlerImpl handler(cq, mock, "test-subscription", "test-ack-id");
  EXPECT_EQ("test-ack-id", handler.ack_id());
  ASSERT_NO_FATAL_FAILURE(handler.nack());
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
