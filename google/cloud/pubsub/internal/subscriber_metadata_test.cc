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

#include "google/cloud/pubsub/internal/subscriber_metadata.h"
#include "google/cloud/pubsub/snapshot.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/pubsub/testing/mock_subscriber_stub.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::testing::_;
using ::testing::Contains;
using ::testing::Not;
using ::testing::Pair;

class SubscriberMetadataTest : public ::testing::Test {
 protected:
  void IsContextMDValid(grpc::ClientContext& context, std::string const& method,
                        google::protobuf::Message const& request) {
    return validate_metadata_fixture_.IsContextMDValid(
        context, method, request, google::cloud::internal::ApiClientHeader());
  }

  void ValidateNoUserProject(grpc::ClientContext& context) {
    auto md = validate_metadata_fixture_.GetMetadata(context);
    EXPECT_THAT(md, Not(Contains(Pair("x-goog-user-project", _))));
  }

  void ValidateTestUserProject(grpc::ClientContext& context) {
    auto md = validate_metadata_fixture_.GetMetadata(context);
    EXPECT_THAT(md, Contains(Pair("x-goog-user-project", "test-project")));
  }

  static Options TestOptions(std::string const& user_project) {
    return user_project.empty()
               ? Options{}
               : Options{}.set<UserProjectOption>(user_project);
  }

 private:
  ValidateMetadataFixture validate_metadata_fixture_;
};

TEST_F(SubscriberMetadataTest, CreateSubscription) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, CreateSubscription)
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::Subscription const& request) {
        IsContextMDValid(
            context, "google.pubsub.v1.Subscriber.CreateSubscription", request);
        return make_status_or(google::pubsub::v1::Subscription{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::Subscription const&) {
        ValidateNoUserProject(context);
        return make_status_or(google::pubsub::v1::Subscription{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::Subscription const&) {
        ValidateTestUserProject(context);
        return make_status_or(google::pubsub::v1::Subscription{});
      });

  SubscriberMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    grpc::ClientContext context;
    google::pubsub::v1::Subscription subscription;
    subscription.set_name(
        pubsub::Subscription("test-project", "test-subscription").FullName());
    auto status = stub.CreateSubscription(context, subscription);
    EXPECT_STATUS_OK(status);
  }
}

TEST_F(SubscriberMetadataTest, GetSubscription) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, GetSubscription)
      .WillOnce([this](
                    grpc::ClientContext& context,
                    google::pubsub::v1::GetSubscriptionRequest const& request) {
        IsContextMDValid(context, "google.pubsub.v1.Subscriber.GetSubscription",
                         request);
        return make_status_or(google::pubsub::v1::Subscription{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::GetSubscriptionRequest const&) {
        ValidateNoUserProject(context);
        return make_status_or(google::pubsub::v1::Subscription{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::GetSubscriptionRequest const&) {
        ValidateTestUserProject(context);
        return make_status_or(google::pubsub::v1::Subscription{});
      });

  SubscriberMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    grpc::ClientContext context;
    google::pubsub::v1::GetSubscriptionRequest request;
    request.set_subscription(
        pubsub::Subscription("test-project", "test-subscription").FullName());
    auto status = stub.GetSubscription(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST_F(SubscriberMetadataTest, UpdateSubscription) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, UpdateSubscription)
      .WillOnce(
          [this](grpc::ClientContext& context,
                 google::pubsub::v1::UpdateSubscriptionRequest const& request) {
            IsContextMDValid(context,
                             "google.pubsub.v1.Subscriber.UpdateSubscription",
                             request);
            return make_status_or(google::pubsub::v1::Subscription{});
          })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::UpdateSubscriptionRequest const&) {
        ValidateNoUserProject(context);
        return make_status_or(google::pubsub::v1::Subscription{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::UpdateSubscriptionRequest const&) {
        ValidateTestUserProject(context);
        return make_status_or(google::pubsub::v1::Subscription{});
      });

  SubscriberMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    grpc::ClientContext context;
    google::pubsub::v1::UpdateSubscriptionRequest request;
    request.mutable_subscription()->set_name(
        pubsub::Subscription("test-project", "test-subscription").FullName());
    auto status = stub.UpdateSubscription(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST_F(SubscriberMetadataTest, ListSubscriptions) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, ListSubscriptions)
      .WillOnce(
          [this](grpc::ClientContext& context,
                 google::pubsub::v1::ListSubscriptionsRequest const& request) {
            IsContextMDValid(context,
                             "google.pubsub.v1.Subscriber.ListSubscriptions",
                             request);
            return make_status_or(
                google::pubsub::v1::ListSubscriptionsResponse{});
          })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::ListSubscriptionsRequest const&) {
        ValidateNoUserProject(context);
        return make_status_or(google::pubsub::v1::ListSubscriptionsResponse{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::ListSubscriptionsRequest const&) {
        ValidateTestUserProject(context);
        return make_status_or(google::pubsub::v1::ListSubscriptionsResponse{});
      });

  SubscriberMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    grpc::ClientContext context;
    google::pubsub::v1::ListSubscriptionsRequest request;
    request.set_project("projects/test-project");
    auto status = stub.ListSubscriptions(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST_F(SubscriberMetadataTest, DeleteSubscription) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, DeleteSubscription)
      .WillOnce(
          [this](grpc::ClientContext& context,
                 google::pubsub::v1::DeleteSubscriptionRequest const& request) {
            IsContextMDValid(context,
                             "google.pubsub.v1.Subscriber.DeleteSubscription",
                             request);
            return Status{};
          })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::DeleteSubscriptionRequest const&) {
        ValidateNoUserProject(context);
        return Status{};
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::DeleteSubscriptionRequest const&) {
        ValidateTestUserProject(context);
        return Status{};
      });

  SubscriberMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    grpc::ClientContext context;
    google::pubsub::v1::DeleteSubscriptionRequest request;
    request.set_subscription(
        pubsub::Subscription("test-project", "test-subscription").FullName());
    auto status = stub.DeleteSubscription(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST_F(SubscriberMetadataTest, ModifyPushConfig) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, ModifyPushConfig)
      .WillOnce(
          [this](grpc::ClientContext& context,
                 google::pubsub::v1::ModifyPushConfigRequest const& request) {
            IsContextMDValid(context,
                             "google.pubsub.v1.Subscriber.ModifyPushConfig",
                             request);
            return Status{};
          })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::ModifyPushConfigRequest const&) {
        ValidateNoUserProject(context);
        return Status{};
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::ModifyPushConfigRequest const&) {
        ValidateTestUserProject(context);
        return Status{};
      });

  SubscriberMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    grpc::ClientContext context;
    google::pubsub::v1::ModifyPushConfigRequest request;
    request.set_subscription(
        pubsub::Subscription("test-project", "test-subscription").FullName());
    auto status = stub.ModifyPushConfig(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST_F(SubscriberMetadataTest, AsyncStreamingPull) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, AsyncStreamingPull)
      .WillOnce(
          [this](google::cloud::CompletionQueue&,
                 std::unique_ptr<grpc::ClientContext> context,
                 google::pubsub::v1::StreamingPullRequest const& request) {
            IsContextMDValid(
                *context, "google.pubsub.v1.Subscriber.StreamingPull", request);
            return absl::make_unique<pubsub_testing::MockAsyncPullStream>();
          })
      .WillOnce([this](google::cloud::CompletionQueue&,
                       std::unique_ptr<grpc::ClientContext> context,
                       google::pubsub::v1::StreamingPullRequest const&) {
        ValidateNoUserProject(*context);
        return absl::make_unique<pubsub_testing::MockAsyncPullStream>();
      })
      .WillOnce([this](google::cloud::CompletionQueue&,
                       std::unique_ptr<grpc::ClientContext> context,
                       google::pubsub::v1::StreamingPullRequest const&) {
        ValidateTestUserProject(*context);
        return absl::make_unique<pubsub_testing::MockAsyncPullStream>();
      });

  SubscriberMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    google::cloud::CompletionQueue cq;
    google::pubsub::v1::StreamingPullRequest request;
    request.set_subscription(
        pubsub::Subscription("test-project", "test-subscription").FullName());
    auto stream = stub.AsyncStreamingPull(
        cq, absl::make_unique<grpc::ClientContext>(), request);
    EXPECT_TRUE(stream);
  }
}

TEST_F(SubscriberMetadataTest, AsyncAcknowledge) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, AsyncAcknowledge)
      .WillOnce([this](google::cloud::CompletionQueue&,
                       std::unique_ptr<grpc::ClientContext> context,
                       google::pubsub::v1::AcknowledgeRequest const& request) {
        IsContextMDValid(*context, "google.pubsub.v1.Subscriber.Acknowledge",
                         request);
        return make_ready_future(Status{});
      })
      .WillOnce([this](google::cloud::CompletionQueue&,
                       std::unique_ptr<grpc::ClientContext> context,
                       google::pubsub::v1::AcknowledgeRequest const&) {
        ValidateNoUserProject(*context);
        return make_ready_future(Status{});
      })
      .WillOnce([this](google::cloud::CompletionQueue&,
                       std::unique_ptr<grpc::ClientContext> context,
                       google::pubsub::v1::AcknowledgeRequest const&) {
        ValidateTestUserProject(*context);
        return make_ready_future(Status{});
      });

  SubscriberMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    google::cloud::CompletionQueue cq;
    google::pubsub::v1::AcknowledgeRequest request;
    request.set_subscription(
        pubsub::Subscription("test-project", "test-subscription").FullName());
    auto response = stub.AsyncAcknowledge(
        cq, absl::make_unique<grpc::ClientContext>(), request);
    EXPECT_STATUS_OK(response.get());
  }
}

TEST_F(SubscriberMetadataTest, AsyncModifyAckDeadline) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, AsyncModifyAckDeadline)
      .WillOnce(
          [this](google::cloud::CompletionQueue&,
                 std::unique_ptr<grpc::ClientContext> context,
                 google::pubsub::v1::ModifyAckDeadlineRequest const& request) {
            IsContextMDValid(*context,
                             "google.pubsub.v1.Subscriber.ModifyAckDeadline",
                             request);
            return make_ready_future(Status{});
          })
      .WillOnce([this](google::cloud::CompletionQueue&,
                       std::unique_ptr<grpc::ClientContext> context,
                       google::pubsub::v1::ModifyAckDeadlineRequest const&) {
        ValidateNoUserProject(*context);
        return make_ready_future(Status{});
      })
      .WillOnce([this](google::cloud::CompletionQueue&,
                       std::unique_ptr<grpc::ClientContext> context,
                       google::pubsub::v1::ModifyAckDeadlineRequest const&) {
        ValidateTestUserProject(*context);
        return make_ready_future(Status{});
      });

  SubscriberMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    google::cloud::CompletionQueue cq;
    google::pubsub::v1::ModifyAckDeadlineRequest request;
    request.set_subscription(
        pubsub::Subscription("test-project", "test-subscription").FullName());
    auto response = stub.AsyncModifyAckDeadline(
        cq, absl::make_unique<grpc::ClientContext>(), request);
    EXPECT_STATUS_OK(response.get());
  }
}

TEST_F(SubscriberMetadataTest, CreateSnapshot) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, CreateSnapshot)
      .WillOnce(
          [this](grpc::ClientContext& context,
                 google::pubsub::v1::CreateSnapshotRequest const& request) {
            IsContextMDValid(
                context, "google.pubsub.v1.Subscriber.CreateSnapshot", request);
            return make_status_or(google::pubsub::v1::Snapshot{});
          })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::CreateSnapshotRequest const&) {
        ValidateNoUserProject(context);
        return make_status_or(google::pubsub::v1::Snapshot{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::CreateSnapshotRequest const&) {
        ValidateTestUserProject(context);
        return make_status_or(google::pubsub::v1::Snapshot{});
      });

  SubscriberMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    grpc::ClientContext context;
    google::pubsub::v1::CreateSnapshotRequest request;
    request.set_name(
        pubsub::Snapshot("test-project", "test-snapshot").FullName());
    auto status = stub.CreateSnapshot(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST_F(SubscriberMetadataTest, GetSnapshot) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, GetSnapshot)
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::GetSnapshotRequest const& request) {
        IsContextMDValid(context, "google.pubsub.v1.Subscriber.GetSnapshot",
                         request);
        return make_status_or(google::pubsub::v1::Snapshot{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::GetSnapshotRequest const&) {
        ValidateNoUserProject(context);
        return make_status_or(google::pubsub::v1::Snapshot{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::GetSnapshotRequest const&) {
        ValidateTestUserProject(context);
        return make_status_or(google::pubsub::v1::Snapshot{});
      });

  SubscriberMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    grpc::ClientContext context;
    google::pubsub::v1::GetSnapshotRequest request;
    request.set_snapshot(
        pubsub::Snapshot("test-project", "test-snapshot").FullName());
    auto status = stub.GetSnapshot(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST_F(SubscriberMetadataTest, ListSnapshots) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, ListSnapshots)
      .WillOnce(
          [this](grpc::ClientContext& context,
                 google::pubsub::v1::ListSnapshotsRequest const& request) {
            IsContextMDValid(
                context, "google.pubsub.v1.Subscriber.ListSnapshots", request);
            return make_status_or(google::pubsub::v1::ListSnapshotsResponse{});
          })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::ListSnapshotsRequest const&) {
        ValidateNoUserProject(context);
        return make_status_or(google::pubsub::v1::ListSnapshotsResponse{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::ListSnapshotsRequest const&) {
        ValidateTestUserProject(context);
        return make_status_or(google::pubsub::v1::ListSnapshotsResponse{});
      });

  SubscriberMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    grpc::ClientContext context;
    google::pubsub::v1::ListSnapshotsRequest request;
    request.set_project("projects/test-project");
    auto status = stub.ListSnapshots(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST_F(SubscriberMetadataTest, UpdateSnapshot) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, UpdateSnapshot)
      .WillOnce(
          [this](grpc::ClientContext& context,
                 google::pubsub::v1::UpdateSnapshotRequest const& request) {
            IsContextMDValid(
                context, "google.pubsub.v1.Subscriber.UpdateSnapshot", request);
            return make_status_or(google::pubsub::v1::Snapshot{});
          })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::UpdateSnapshotRequest const&) {
        ValidateNoUserProject(context);
        return make_status_or(google::pubsub::v1::Snapshot{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::UpdateSnapshotRequest const&) {
        ValidateTestUserProject(context);
        return make_status_or(google::pubsub::v1::Snapshot{});
      });

  SubscriberMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    grpc::ClientContext context;
    google::pubsub::v1::UpdateSnapshotRequest request;
    request.mutable_snapshot()->set_name(
        pubsub::Snapshot("test-project", "test-snapshot").FullName());
    auto status = stub.UpdateSnapshot(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST_F(SubscriberMetadataTest, DeleteSnapshot) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, DeleteSnapshot)
      .WillOnce(
          [this](grpc::ClientContext& context,
                 google::pubsub::v1::DeleteSnapshotRequest const& request) {
            IsContextMDValid(
                context, "google.pubsub.v1.Subscriber.DeleteSnapshot", request);
            return Status{};
          })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::DeleteSnapshotRequest const&) {
        ValidateNoUserProject(context);
        return Status{};
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::DeleteSnapshotRequest const&) {
        ValidateTestUserProject(context);
        return Status{};
      });

  SubscriberMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    grpc::ClientContext context;
    google::pubsub::v1::DeleteSnapshotRequest request;
    request.set_snapshot(
        pubsub::Snapshot("test-project", "test-snapshot").FullName());
    auto status = stub.DeleteSnapshot(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST_F(SubscriberMetadataTest, Seek) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, Seek)
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::SeekRequest const& request) {
        IsContextMDValid(context, "google.pubsub.v1.Subscriber.Seek", request);
        return make_status_or(google::pubsub::v1::SeekResponse{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::SeekRequest const&) {
        ValidateNoUserProject(context);
        return make_status_or(google::pubsub::v1::SeekResponse{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::SeekRequest const&) {
        ValidateTestUserProject(context);
        return make_status_or(google::pubsub::v1::SeekResponse{});
      });

  SubscriberMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    grpc::ClientContext context;
    google::pubsub::v1::SeekRequest request;
    request.set_subscription(
        pubsub::Subscription("test-project", "test-subscription").FullName());
    auto status = stub.Seek(context, request);
    EXPECT_STATUS_OK(status);
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
