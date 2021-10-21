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

#include "google/cloud/pubsub/internal/sequential_batch_sink.h"
#include "google/cloud/pubsub/testing/mock_batch_sink.h"
#include "google/cloud/pubsub/testing/test_retry_policies.h"
#include "google/cloud/pubsub/topic.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <condition_variable>
#include <deque>
#include <mutex>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Unused;

pubsub::Topic TestTopic() {
  return pubsub::Topic("test-project", "test-topic");
}

std::string TestOrderingKey() { return "test-key"; }

google::pubsub::v1::PublishRequest MakeRequest(int n) {
  google::pubsub::v1::PublishRequest request;
  request.set_topic(TestTopic().FullName());

  for (int i = 0; i != n; ++i) {
    auto& m = *request.add_messages();
    m.set_message_id("message-" + std::to_string(i));
    m.set_ordering_key(TestOrderingKey());
  }
  return request;
}

google::pubsub::v1::PublishResponse MakeResponse(
    google::pubsub::v1::PublishRequest const& request) {
  google::pubsub::v1::PublishResponse response;
  for (auto const& m : request.messages()) {
    response.add_message_ids("id-" + m.message_id());
  }
  return response;
}

TEST(DefaultBatchSinkTest, BasicNoErrors) {
  AsyncSequencer<void> sequencer;

  // We will use this function to ensure the mock calls happen *after* some
  // code below.
  ::testing::MockFunction<void(int)> barrier;

  auto mock = std::make_shared<pubsub_testing::MockBatchSink>();
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(barrier, Call(0));
    EXPECT_CALL(*mock, AsyncPublish)
        .WillOnce([&](google::pubsub::v1::PublishRequest const& r) {
          EXPECT_THAT(r, IsProtoEqual(MakeRequest(3)));
          return sequencer.PushBack().then(
              [r](future<void>) { return make_status_or(MakeResponse(r)); });
        });

    EXPECT_CALL(barrier, Call(1));
    EXPECT_CALL(*mock, AsyncPublish)
        .WillOnce([&](google::pubsub::v1::PublishRequest const& r) {
          EXPECT_THAT(r, IsProtoEqual(MakeRequest(2)));
          return sequencer.PushBack().then(
              [r](future<void>) { return make_status_or(MakeResponse(r)); });
        });

    EXPECT_CALL(barrier, Call(2));
    EXPECT_CALL(*mock, AsyncPublish)
        .WillOnce([&](google::pubsub::v1::PublishRequest const& r) {
          EXPECT_THAT(r, IsProtoEqual(MakeRequest(1)));
          return sequencer.PushBack().then(
              [r](future<void>) { return make_status_or(MakeResponse(r)); });
        });
  }

  auto checkpoint = barrier.AsStdFunction();
  auto uut = SequentialBatchSink::Create(mock);
  // We only expect the first AsyncPublish() to be called.
  checkpoint(0);
  auto f1 = uut->AsyncPublish(MakeRequest(3));
  auto f2 = uut->AsyncPublish(MakeRequest(2));
  auto f3 = uut->AsyncPublish(MakeRequest(1));
  EXPECT_EQ(2, uut->QueueDepth());

  checkpoint(1);
  sequencer.PopFront().set_value();
  auto r1 = f1.get();
  ASSERT_THAT(r1, IsOk());
  EXPECT_THAT(*r1, IsProtoEqual(MakeResponse(MakeRequest(3))));
  EXPECT_EQ(1, uut->QueueDepth());

  checkpoint(2);
  sequencer.PopFront().set_value();
  auto r2 = f2.get();
  ASSERT_THAT(r2, IsOk());
  EXPECT_THAT(*r2, IsProtoEqual(MakeResponse(MakeRequest(2))));
  EXPECT_EQ(0, uut->QueueDepth());
}

TEST(DefaultBatchSinkTest, BasicErrorHandling) {
  AsyncSequencer<void> sequencer;

  auto mock = std::make_shared<pubsub_testing::MockBatchSink>();
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*mock, AsyncPublish)
        .WillOnce([&](google::pubsub::v1::PublishRequest const& r) {
          EXPECT_THAT(r, IsProtoEqual(MakeRequest(2)));
          return sequencer.PushBack().then([r](future<void>) {
            return StatusOr<google::pubsub::v1::PublishResponse>(
                Status{StatusCode::kPermissionDenied, "uh-oh"});
          });
        });

    EXPECT_CALL(*mock, ResumePublish);
    EXPECT_CALL(*mock, AsyncPublish)
        .WillOnce([&](google::pubsub::v1::PublishRequest const& r) {
          EXPECT_THAT(r, IsProtoEqual(MakeRequest(2)));
          return sequencer.PushBack().then(
              [r](future<void>) { return make_status_or(MakeResponse(r)); });
        });
  }

  auto uut = SequentialBatchSink::Create(mock);
  auto f1 = uut->AsyncPublish(MakeRequest(2));
  auto f2 = uut->AsyncPublish(MakeRequest(3));
  auto f3 = uut->AsyncPublish(MakeRequest(3));
  EXPECT_EQ(2, uut->QueueDepth());

  sequencer.PopFront().set_value();
  auto r1 = f1.get();
  ASSERT_THAT(r1, StatusIs(StatusCode::kPermissionDenied));

  // The queued messages should become satisfied with the same error status.
  auto r2 = f2.get();
  ASSERT_THAT(r2, StatusIs(StatusCode::kPermissionDenied));
  auto r3 = f3.get();
  ASSERT_THAT(r3, StatusIs(StatusCode::kPermissionDenied));

  // And new messages should become satisfied with the same error too.
  auto r4 = uut->AsyncPublish(MakeRequest(3)).get();
  ASSERT_THAT(r4, StatusIs(StatusCode::kPermissionDenied));

  // Calling ResumePublish() enables regular messages again.
  uut->ResumePublish(TestOrderingKey());
  auto f5 = uut->AsyncPublish(MakeRequest(2));
  sequencer.PopFront().set_value();
  auto r5 = f5.get();
  ASSERT_THAT(r5, IsOk());
  EXPECT_THAT(*r5, IsProtoEqual(MakeResponse(MakeRequest(2))));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
