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

#include "google/cloud/pubsub/internal/publisher_round_robin.h"
#include "google/cloud/pubsub/testing/mock_publisher_stub.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <algorithm>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::testing::InSequence;
using ::testing::Return;

auto constexpr kMockCount = 3;
auto constexpr kRepeats = 2;

std::vector<std::shared_ptr<pubsub_testing::MockPublisherStub>> MakeMocks() {
  std::vector<std::shared_ptr<pubsub_testing::MockPublisherStub>> mocks(
      kMockCount);
  std::generate(mocks.begin(), mocks.end(), [] {
    return std::make_shared<pubsub_testing::MockPublisherStub>();
  });
  return mocks;
}

std::vector<std::shared_ptr<PublisherStub>> AsPlainStubs(
    std::vector<std::shared_ptr<pubsub_testing::MockPublisherStub>> mocks) {
  return std::vector<std::shared_ptr<PublisherStub>>(mocks.begin(),
                                                     mocks.end());
}

TEST(PublisherRoundRobinTest, CreateTopic) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, CreateTopic)
          .WillOnce(Return(make_status_or(google::pubsub::v1::Topic{})));
    }
  }
  PublisherRoundRobin stub(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::pubsub::v1::Topic topic;
    topic.set_name("test-topic-name");
    auto status = stub.CreateTopic(context, topic);
    EXPECT_STATUS_OK(status);
  }
}

TEST(PublisherRoundRobinTest, GetTopic) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, GetTopic)
          .WillOnce(Return(make_status_or(google::pubsub::v1::Topic{})));
    }
  }
  PublisherRoundRobin stub(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::pubsub::v1::GetTopicRequest request;
    request.set_topic("test-topic-name");
    auto status = stub.GetTopic(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST(PublisherRoundRobinTest, UpdateTopic) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, UpdateTopic)
          .WillOnce(Return(make_status_or(google::pubsub::v1::Topic{})));
    }
  }
  PublisherRoundRobin stub(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::pubsub::v1::UpdateTopicRequest request;
    request.mutable_topic()->set_name("test-topic-name");
    auto status = stub.UpdateTopic(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST(PublisherRoundRobinTest, ListTopics) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, ListTopics)
          .WillOnce(
              Return(make_status_or(google::pubsub::v1::ListTopicsResponse{})));
    }
  }
  PublisherRoundRobin stub(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::pubsub::v1::ListTopicsRequest request;
    request.set_project("test-project-name");
    auto status = stub.ListTopics(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST(PublisherRoundRobinTest, DeleteTopic) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, DeleteTopic).WillOnce(Return(Status{}));
    }
  }
  PublisherRoundRobin stub(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::pubsub::v1::DeleteTopicRequest request;
    request.set_topic("test-topic-name");
    auto status = stub.DeleteTopic(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST(PublisherRoundRobinTest, DetachSubscription) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, DetachSubscription)
          .WillOnce(Return(make_status_or(
              google::pubsub::v1::DetachSubscriptionResponse{})));
    }
  }
  PublisherRoundRobin stub(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::pubsub::v1::DetachSubscriptionRequest request;
    request.set_subscription("test-subscription-name");
    auto status = stub.DetachSubscription(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST(PublisherRoundRobinTest, ListTopicSubscriptions) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, ListTopicSubscriptions)
          .WillOnce(Return(make_status_or(
              google::pubsub::v1::ListTopicSubscriptionsResponse{})));
    }
  }
  PublisherRoundRobin stub(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::pubsub::v1::ListTopicSubscriptionsRequest request;
    request.set_topic("test-topic-name");
    auto status = stub.ListTopicSubscriptions(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST(PublisherRoundRobinTest, ListTopicSnapshots) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, ListTopicSnapshots)
          .WillOnce(Return(make_status_or(
              google::pubsub::v1::ListTopicSnapshotsResponse{})));
    }
  }
  PublisherRoundRobin stub(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::pubsub::v1::ListTopicSnapshotsRequest request;
    request.set_topic("test-topic-name");
    auto status = stub.ListTopicSnapshots(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST(PublisherRoundRobinTest, AsyncPublish) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, AsyncPublish)
          .WillOnce([](google::cloud::CompletionQueue&,
                       std::unique_ptr<grpc::ClientContext>,
                       google::pubsub::v1::PublishRequest const&) {
            return make_ready_future(
                make_status_or(google::pubsub::v1::PublishResponse{}));
          });
    }
  }
  PublisherRoundRobin stub(AsPlainStubs(mocks));
  for (size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::cloud::CompletionQueue cq;
    google::pubsub::v1::PublishRequest request;
    request.set_topic("test-topic-name");
    auto status =
        stub.AsyncPublish(cq, absl::make_unique<grpc::ClientContext>(), request)
            .get();
    EXPECT_STATUS_OK(status);
  }
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
