// Copyright 2020 Google LLC
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

#include "google/cloud/pubsub/internal/default_batch_sink.h"
#include "google/cloud/pubsub/internal/defaults.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/pubsub/testing/mock_publisher_stub.h"
#include "google/cloud/pubsub/testing/test_retry_policies.h"
#include "google/cloud/pubsub/topic.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AtLeast;
using ::testing::HasSubstr;
using ::testing::Unused;

std::shared_ptr<DefaultBatchSink> MakeTestBatchSink(
    std::shared_ptr<PublisherStub> mock, CompletionQueue cq) {
  return DefaultBatchSink::Create(
      std::move(mock), std::move(cq),
      DefaultPublisherOptions(pubsub_testing::MakeTestOptions()));
}

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
      .WillOnce([] {
        return make_ready_future(StatusOr<google::pubsub::v1::PublishResponse>(
            Status{StatusCode::kUnavailable, "try-again"}));
      })
      .WillOnce([](Unused, Unused, Unused,
                   google::pubsub::v1::PublishRequest const& request) {
        EXPECT_THAT(request, IsProtoEqual(MakeRequest(3)));
        return make_ready_future(make_status_or(MakeResponse(request)));
      });

  internal::AutomaticallyCreatedBackgroundThreads background;
  auto uut = MakeTestBatchSink(mock, background.cq());

  auto response = uut->AsyncPublish(MakeRequest(3)).get();
  ASSERT_THAT(response, IsOk());
  EXPECT_THAT(*response, IsProtoEqual(MakeResponse(MakeRequest(3))));

  uut->ResumePublish("unused");  // No observable side-effects
}

TEST(DefaultBatchSinkTest, PermanentError) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, AsyncPublish).WillOnce([] {
    return make_ready_future(StatusOr<google::pubsub::v1::PublishResponse>(
        Status{StatusCode::kPermissionDenied, "uh-oh"}));
  });

  internal::AutomaticallyCreatedBackgroundThreads background;
  auto uut = MakeTestBatchSink(mock, background.cq());

  auto response = uut->AsyncPublish(MakeRequest(3)).get();
  ASSERT_THAT(response,
              StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
}

TEST(DefaultBatchSinkTest, TooManyTransients) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, AsyncPublish).Times(AtLeast(2)).WillRepeatedly([] {
    return make_ready_future(StatusOr<google::pubsub::v1::PublishResponse>(
        Status{StatusCode::kUnavailable, "try-again"}));
  });

  internal::AutomaticallyCreatedBackgroundThreads background;
  auto uut = MakeTestBatchSink(mock, background.cq());

  auto response = uut->AsyncPublish(MakeRequest(3)).get();
  ASSERT_THAT(response,
              StatusIs(StatusCode::kUnavailable, HasSubstr("try-again")));
}

TEST(DefaultBatchSinkTest, BasicWithCompression) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([](Unused, auto context, Unused,
                   google::pubsub::v1::PublishRequest const& request) {
        // The pubsub::CompressionAlgorithmOption takes precedence over
        // GrpcCompressionAlgorithmOption when the former's threshold is
        // met.
        EXPECT_EQ(context->compression_algorithm(), GRPC_COMPRESS_GZIP);
        EXPECT_THAT(request, IsProtoEqual(MakeRequest(3)));
        return make_ready_future(make_status_or(MakeResponse(request)));
      });

  internal::AutomaticallyCreatedBackgroundThreads background;
  auto uut = DefaultBatchSink::Create(
      std::move(mock), background.cq(),
      DefaultPublisherOptions(
          pubsub_testing::MakeTestOptions()
              .set<GrpcCompressionAlgorithmOption>(GRPC_COMPRESS_NONE)
              .set<pubsub::CompressionThresholdOption>(0)
              .set<pubsub::CompressionAlgorithmOption>(GRPC_COMPRESS_GZIP)));

  auto response = uut->AsyncPublish(MakeRequest(3)).get();
  ASSERT_THAT(response, IsOk());
  EXPECT_THAT(*response, IsProtoEqual(MakeResponse(MakeRequest(3))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
