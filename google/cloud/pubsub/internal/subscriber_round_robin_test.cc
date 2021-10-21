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

#include "google/cloud/pubsub/internal/subscriber_round_robin.h"
#include "google/cloud/pubsub/testing/mock_subscriber_stub.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::testing::InSequence;
using ::testing::Return;

auto constexpr kMockCount = 3;
auto constexpr kRepeats = 2;

std::vector<std::shared_ptr<pubsub_testing::MockSubscriberStub>> MakeMocks() {
  std::vector<std::shared_ptr<pubsub_testing::MockSubscriberStub>> mocks(
      kMockCount);
  std::generate(mocks.begin(), mocks.end(), [] {
    return std::make_shared<pubsub_testing::MockSubscriberStub>();
  });
  return mocks;
}

std::vector<std::shared_ptr<SubscriberStub>> AsPlainStubs(
    std::vector<std::shared_ptr<pubsub_testing::MockSubscriberStub>> mocks) {
  return std::vector<std::shared_ptr<SubscriberStub>>(mocks.begin(),
                                                      mocks.end());
}

TEST(SubscriberRoundRobinTest, CreateSubscription) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, CreateSubscription)
          .WillOnce(Return(make_status_or(google::pubsub::v1::Subscription{})));
    }
  }
  SubscriberRoundRobin stub(AsPlainStubs(mocks));
  for (std::size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::pubsub::v1::Subscription subscription;
    auto status = stub.CreateSubscription(context, subscription);
    EXPECT_STATUS_OK(status);
  }
}

TEST(SubscriberRoundRobinTest, GetSubscription) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, GetSubscription)
          .WillOnce(Return(make_status_or(google::pubsub::v1::Subscription{})));
    }
  }
  SubscriberRoundRobin stub(AsPlainStubs(mocks));
  for (std::size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::pubsub::v1::GetSubscriptionRequest request;
    auto status = stub.GetSubscription(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST(SubscriberRoundRobinTest, UpdateSubscription) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, UpdateSubscription)
          .WillOnce(Return(make_status_or(google::pubsub::v1::Subscription{})));
    }
  }
  SubscriberRoundRobin stub(AsPlainStubs(mocks));
  for (std::size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::pubsub::v1::UpdateSubscriptionRequest request;
    auto status = stub.UpdateSubscription(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST(SubscriberRoundRobinTest, ListSubscriptions) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, ListSubscriptions)
          .WillOnce(Return(
              make_status_or(google::pubsub::v1::ListSubscriptionsResponse{})));
    }
  }
  SubscriberRoundRobin stub(AsPlainStubs(mocks));
  for (std::size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::pubsub::v1::ListSubscriptionsRequest request;
    request.set_project("test-project-name");
    auto status = stub.ListSubscriptions(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST(SubscriberRoundRobinTest, DeleteSubscription) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, DeleteSubscription).WillOnce(Return(Status{}));
    }
  }
  SubscriberRoundRobin stub(AsPlainStubs(mocks));
  for (std::size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::pubsub::v1::DeleteSubscriptionRequest request;
    request.set_subscription("test-subscription-name");
    auto status = stub.DeleteSubscription(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST(SubscriberRoundRobinTest, ModifyPushConfig) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, ModifyPushConfig).WillOnce(Return(Status{}));
    }
  }
  SubscriberRoundRobin stub(AsPlainStubs(mocks));
  for (std::size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::pubsub::v1::ModifyPushConfigRequest request;
    request.set_subscription("test-subscription-name");
    auto status = stub.ModifyPushConfig(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST(SubscriberRoundRobinTest, AsyncStreamingPull) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, AsyncStreamingPull)
          .WillOnce([](google::cloud::CompletionQueue&,
                       std::unique_ptr<grpc::ClientContext>,
                       google::pubsub::v1::StreamingPullRequest const&) {
            return absl::make_unique<pubsub_testing::MockAsyncPullStream>();
          });
    }
  }
  SubscriberRoundRobin stub(AsPlainStubs(mocks));
  google::cloud::CompletionQueue cq;
  for (std::size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::pubsub::v1::StreamingPullRequest request;
    request.set_subscription("test-subscription-name");
    auto stream = stub.AsyncStreamingPull(
        cq, absl::make_unique<grpc::ClientContext>(), request);
  }
}

TEST(SubscriberRoundRobinTest, AsyncAcknowledge) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, AsyncAcknowledge)
          .WillOnce([](google::cloud::CompletionQueue&,
                       std::unique_ptr<grpc::ClientContext>,
                       google::pubsub::v1::AcknowledgeRequest const&) {
            return make_ready_future(Status{});
          });
    }
  }
  CompletionQueue cq;
  SubscriberRoundRobin stub(AsPlainStubs(mocks));
  for (std::size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::pubsub::v1::AcknowledgeRequest request;
    auto status = stub.AsyncAcknowledge(
                          cq, absl::make_unique<grpc::ClientContext>(), request)
                      .get();
    EXPECT_STATUS_OK(status);
  }
}

TEST(SubscriberRoundRobinTest, AsyncModifyAckDeadline) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, AsyncModifyAckDeadline)
          .WillOnce([](google::cloud::CompletionQueue&,
                       std::unique_ptr<grpc::ClientContext>,
                       google::pubsub::v1::ModifyAckDeadlineRequest const&) {
            return make_ready_future(Status{});
          });
    }
  }
  CompletionQueue cq;
  SubscriberRoundRobin stub(AsPlainStubs(mocks));
  for (std::size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    google::pubsub::v1::ModifyAckDeadlineRequest request;
    auto status = stub.AsyncModifyAckDeadline(
                          cq, absl::make_unique<grpc::ClientContext>(), request)
                      .get();
    EXPECT_STATUS_OK(status);
  }
}

TEST(SubscriberRoundRobinTest, CreateSnapshot) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, CreateSnapshot)
          .WillOnce(Return(make_status_or(google::pubsub::v1::Snapshot{})));
    }
  }
  SubscriberRoundRobin stub(AsPlainStubs(mocks));
  for (std::size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::pubsub::v1::CreateSnapshotRequest request;
    auto status = stub.CreateSnapshot(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST(SubscriberRoundRobinTest, GetSnapshot) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, GetSnapshot)
          .WillOnce(Return(make_status_or(google::pubsub::v1::Snapshot{})));
    }
  }
  SubscriberRoundRobin stub(AsPlainStubs(mocks));
  for (std::size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::pubsub::v1::GetSnapshotRequest request;
    auto status = stub.GetSnapshot(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST(SubscriberRoundRobinTest, ListSnapshots) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, ListSnapshots)
          .WillOnce(Return(
              make_status_or(google::pubsub::v1::ListSnapshotsResponse{})));
    }
  }
  SubscriberRoundRobin stub(AsPlainStubs(mocks));
  for (std::size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::pubsub::v1::ListSnapshotsRequest request;
    request.set_project("test-project-name");
    auto status = stub.ListSnapshots(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST(SubscriberRoundRobinTest, UpdateSnapshot) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, UpdateSnapshot)
          .WillOnce(Return(make_status_or(google::pubsub::v1::Snapshot{})));
    }
  }
  SubscriberRoundRobin stub(AsPlainStubs(mocks));
  for (std::size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::pubsub::v1::UpdateSnapshotRequest request;
    auto status = stub.UpdateSnapshot(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST(SubscriberRoundRobinTest, DeleteSnapshot) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, DeleteSnapshot).WillOnce(Return(Status{}));
    }
  }
  SubscriberRoundRobin stub(AsPlainStubs(mocks));
  for (std::size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::pubsub::v1::DeleteSnapshotRequest request;
    auto status = stub.DeleteSnapshot(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST(SubscriberRoundRobinTest, Seek) {
  auto mocks = MakeMocks();
  InSequence sequence;
  for (int i = 0; i != kRepeats; ++i) {
    for (auto& m : mocks) {
      EXPECT_CALL(*m, Seek).WillOnce(
          Return(make_status_or(google::pubsub::v1::SeekResponse{})));
    }
  }
  SubscriberRoundRobin stub(AsPlainStubs(mocks));
  for (std::size_t i = 0; i != kRepeats * mocks.size(); ++i) {
    grpc::ClientContext context;
    google::pubsub::v1::SeekRequest request;
    request.set_subscription("test-subscription-name");
    auto status = stub.Seek(context, request);
    EXPECT_STATUS_OK(status);
  }
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
