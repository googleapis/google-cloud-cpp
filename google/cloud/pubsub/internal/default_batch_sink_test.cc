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

#include "google/cloud/pubsub/internal/default_batch_sink.h"
#include "google/cloud/pubsub/testing/mock_publisher_stub.h"
#include "google/cloud/pubsub/testing/test_retry_policies.h"
#include "google/cloud/pubsub/topic.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AtLeast;
using ::testing::HasSubstr;
using ::testing::Unused;

pubsub::Topic TestTopic() {
  return pubsub::Topic("test-project", "test-topic");
}

google::pubsub::v1::PublishRequest MakeRequest(int n) {
  google::pubsub::v1::PublishRequest request;
  request.set_topic(TestTopic().FullName());
  for (int i = 0; i != n; ++i) {
    request.add_messages()->set_message_id("message-" + std::to_string(i));
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

TEST(DefaultBatchSinkTest, BasicWithRetry) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([](Unused, Unused, Unused) {
        return make_ready_future(StatusOr<google::pubsub::v1::PublishResponse>(
            Status{StatusCode::kUnavailable, "try-again"}));
      })
      .WillOnce([](Unused, Unused,
                   google::pubsub::v1::PublishRequest const& request) {
        EXPECT_THAT(request, IsProtoEqual(MakeRequest(3)));
        return make_ready_future(make_status_or(MakeResponse(request)));
      });

  internal::AutomaticallyCreatedBackgroundThreads background;
  auto uut = DefaultBatchSink::Create(mock, background.cq(),
                                      pubsub_testing::TestRetryPolicy(),
                                      pubsub_testing::TestBackoffPolicy());

  auto response = uut->AsyncPublish(MakeRequest(3)).get();
  ASSERT_THAT(response, StatusIs(StatusCode::kOk));
  EXPECT_THAT(*response, IsProtoEqual(MakeResponse(MakeRequest(3))));

  uut->ResumePublish("unused");  // No observable side-effects
}

TEST(DefaultBatchSinkTest, PermanentError) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, AsyncPublish).WillOnce([](Unused, Unused, Unused) {
    return make_ready_future(StatusOr<google::pubsub::v1::PublishResponse>(
        Status{StatusCode::kPermissionDenied, "uh-oh"}));
  });

  internal::AutomaticallyCreatedBackgroundThreads background;
  auto uut = DefaultBatchSink::Create(mock, background.cq(),
                                      pubsub_testing::TestRetryPolicy(),
                                      pubsub_testing::TestBackoffPolicy());

  auto response = uut->AsyncPublish(MakeRequest(3)).get();
  ASSERT_THAT(response,
              StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
}

TEST(DefaultBatchSinkTest, TooManyTransients) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, AsyncPublish)
      .Times(AtLeast(2))
      .WillRepeatedly([](Unused, Unused, Unused) {
        return make_ready_future(StatusOr<google::pubsub::v1::PublishResponse>(
            Status{StatusCode::kUnavailable, "try-again"}));
      });

  internal::AutomaticallyCreatedBackgroundThreads background;
  auto uut = DefaultBatchSink::Create(mock, background.cq(),
                                      pubsub_testing::TestRetryPolicy(),
                                      pubsub_testing::TestBackoffPolicy());

  auto response = uut->AsyncPublish(MakeRequest(3)).get();
  ASSERT_THAT(response,
              StatusIs(StatusCode::kUnavailable, HasSubstr("try-again")));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
