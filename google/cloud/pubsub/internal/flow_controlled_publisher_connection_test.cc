// Copyright 2021 Google LLC
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

#include "google/cloud/pubsub/internal/flow_controlled_publisher_connection.h"
#include "google/cloud/pubsub/mocks/mock_publisher_connection.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <algorithm>
#include <vector>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::pubsub_mocks::MockPublisherConnection;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;

pubsub::Message MakeTestMessage(std::size_t size) {
  return pubsub::MessageBuilder{}.SetData(std::string(size, 'A')).Build();
}

TEST(FlowControlledPublisherConnection, FullPublisherIgnored) {
  AsyncSequencer<StatusOr<std::string>> publish;
  auto mock = std::make_shared<MockPublisherConnection>();
  EXPECT_CALL(*mock, Publish)
      .WillRepeatedly(
          [&publish](pubsub::PublisherConnection::PublishParams const&) {
            return publish.PushBack("Publish()");
          });
  EXPECT_CALL(*mock, Flush).Times(1);
  EXPECT_CALL(*mock, ResumePublish).Times(1);

  auto under_test = FlowControlledPublisherConnection::Create(
      pubsub::PublisherOptions{}
          .set_maximum_pending_bytes(128 * 1024)
          .set_maximum_pending_messages(8)
          .set_full_publisher_ignored(),
      mock);
  under_test->Flush({});
  under_test->ResumePublish({"test-ordering-key"});
  std::vector<future<StatusOr<std::string>>> pending;
  for (int i = 0; i != 16; ++i) {
    pending.push_back(under_test->Publish({MakeTestMessage(i * 1024 + 1024)}));
  }
  for (auto& p : pending) {
    publish.PopFront().set_value(make_status_or(std::string{"ack"}));
    EXPECT_THAT(p.get(), IsOk());
  }
}

TEST(FlowControlledPublisherConnection, RejectOnBytes) {
  AsyncSequencer<StatusOr<std::string>> publish;
  auto mock = std::make_shared<MockPublisherConnection>();
  EXPECT_CALL(*mock, Publish)
      .WillRepeatedly(
          [&publish](pubsub::PublisherConnection::PublishParams const&) {
            return publish.PushBack("Publish()");
          });

  auto under_test = FlowControlledPublisherConnection::Create(
      pubsub::PublisherOptions{}
          .set_full_publisher_rejects()
          .set_maximum_pending_bytes(128 * 1024),
      mock);
  auto m0 = under_test->Publish({MakeTestMessage(64 * 1024)});
  auto m1 = under_test->Publish({MakeTestMessage(64 * 1024)});
  EXPECT_THAT(m1.get(), StatusIs(StatusCode::kFailedPrecondition));
  publish.PopFront().set_value(make_status_or(std::string{"ack-m0"}));
  EXPECT_THAT(m0.get(), IsOk());

  auto m2 = under_test->Publish({MakeTestMessage(64 * 1024)});
  publish.PopFront().set_value(make_status_or(std::string{"ack-m2"}));
  EXPECT_THAT(m2.get(), IsOk());
}

TEST(FlowControlledPublisherConnection, RejectOnMessages) {
  AsyncSequencer<StatusOr<std::string>> publish;
  auto mock = std::make_shared<MockPublisherConnection>();
  EXPECT_CALL(*mock, Publish)
      .WillRepeatedly(
          [&publish](pubsub::PublisherConnection::PublishParams const&) {
            return publish.PushBack("Publish()");
          });

  auto under_test = FlowControlledPublisherConnection::Create(
      pubsub::PublisherOptions{}
          .set_full_publisher_rejects()
          .set_maximum_pending_messages(4)
          .set_maximum_pending_bytes(128 * 1024),
      mock);

  auto m0 = under_test->Publish({MakeTestMessage(128)});
  auto m1 = under_test->Publish({MakeTestMessage(128)});
  auto m2 = under_test->Publish({MakeTestMessage(128)});
  auto m3 = under_test->Publish({MakeTestMessage(128)});
  auto m = under_test->Publish({MakeTestMessage(128)});
  EXPECT_THAT(m.get(), StatusIs(StatusCode::kFailedPrecondition));
  publish.PopFront().set_value(make_status_or(std::string{"ack-m0"}));
  publish.PopFront().set_value(make_status_or(std::string{"ack-m1"}));
  EXPECT_THAT(m0.get(), IsOk());
  EXPECT_THAT(m1.get(), IsOk());

  auto m4 = under_test->Publish({MakeTestMessage(128)});
  publish.PopFront().set_value(make_status_or(std::string{"ack-m2"}));
  publish.PopFront().set_value(make_status_or(std::string{"ack-m3"}));
  publish.PopFront().set_value(make_status_or(std::string{"ack-m4"}));
  EXPECT_THAT(m2.get(), IsOk());
  EXPECT_THAT(m3.get(), IsOk());
  EXPECT_THAT(m4.get(), IsOk());
}

TEST(FlowControlledPublisherConnection, AcceptsAtLeastOne) {
  AsyncSequencer<StatusOr<std::string>> publish;
  auto mock = std::make_shared<MockPublisherConnection>();
  EXPECT_CALL(*mock, Publish)
      .WillRepeatedly(
          [&publish](pubsub::PublisherConnection::PublishParams const&) {
            return publish.PushBack("Publish()");
          });

  auto under_test = FlowControlledPublisherConnection::Create(
      pubsub::PublisherOptions{}
          .set_full_publisher_rejects()
          .set_maximum_pending_messages(0)
          .set_maximum_pending_bytes(0),
      mock);

  auto m0 = under_test->Publish({MakeTestMessage(128)});
  auto rejected = under_test->Publish({MakeTestMessage(128)});
  EXPECT_THAT(rejected.get(), StatusIs(StatusCode::kFailedPrecondition));
  publish.PopFront().set_value(make_status_or(std::string{"ack-m0"}));
  EXPECT_THAT(m0.get(), IsOk());
  auto m1 = under_test->Publish({MakeTestMessage(128)});
  publish.PopFront().set_value(make_status_or(std::string{"ack-m1"}));
  EXPECT_THAT(m1.get(), IsOk());
}

auto constexpr kMessageSize = 1024;
auto constexpr kExpectedMaxMessages = 4;
auto constexpr kExpectedMaxBytes = kExpectedMaxMessages * kMessageSize;

std::shared_ptr<FlowControlledPublisherConnection> TestFlowControl(
    pubsub::PublisherOptions options) {
  AsyncSequencer<StatusOr<std::string>> publish;
  auto mock = std::make_shared<MockPublisherConnection>();
  EXPECT_CALL(*mock, Publish)
      .WillRepeatedly(
          [&publish](pubsub::PublisherConnection::PublishParams const&) {
            return publish.PushBack("Publish()");
          });

  auto under_test = FlowControlledPublisherConnection::Create(options, mock);

  auto publisher_task = [&](int iterations) {
    for (int i = 0; i != iterations; ++i) {
      under_test->Publish({MakeTestMessage(kMessageSize)});
    }
  };

  auto constexpr kThreadCount = 8;
  auto constexpr kIterationCount = 128;
  std::vector<std::thread> tasks(kThreadCount);
  std::generate(tasks.begin(), tasks.end(),
                [&] { return std::thread(publisher_task, kIterationCount); });

  for (int i = 0; i != kThreadCount * kIterationCount; ++i) {
    auto id = "fake-ack-" + std::to_string(i);
    publish.PopFront().set_value(make_status_or(std::move(id)));
  }
  for (auto& t : tasks) t.join();
  return under_test;
}

TEST(FlowControlledPublisherConnection, BlockOnBytes) {
  auto const actual =
      TestFlowControl(pubsub::PublisherOptions{}
                          .set_full_publisher_blocks()
                          .set_maximum_pending_bytes(kExpectedMaxBytes));
  EXPECT_LE(actual->max_pending_bytes(), kExpectedMaxBytes);
}

TEST(FlowControlledPublisherConnection, BlockOnMessages) {
  auto const actual =
      TestFlowControl(pubsub::PublisherOptions{}
                          .set_full_publisher_blocks()
                          .set_maximum_pending_messages(kExpectedMaxMessages));
  EXPECT_LE(actual->max_pending_messages(), kExpectedMaxMessages);
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
