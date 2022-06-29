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

#include "google/cloud/pubsub/internal/publisher_metadata.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/pubsub/testing/mock_publisher_stub.h"
#include "google/cloud/pubsub/topic.h"
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

class PublisherMetadataTest : public ::testing::Test {
 protected:
  template <typename Request>
  void IsContextMDValid(grpc::ClientContext& context, std::string const& method,
                        Request const& request) {
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

TEST_F(PublisherMetadataTest, CreateTopic) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, CreateTopic)
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::Topic const& request) {
        IsContextMDValid(context, "google.pubsub.v1.Publisher.CreateTopic",
                         request);
        return make_status_or(google::pubsub::v1::Topic{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::Topic const&) {
        ValidateNoUserProject(context);
        return make_status_or(google::pubsub::v1::Topic{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::Topic const&) {
        ValidateTestUserProject(context);
        return make_status_or(google::pubsub::v1::Topic{});
      });

  PublisherMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    grpc::ClientContext context;
    google::pubsub::v1::Topic topic;
    topic.set_name(pubsub::Topic("test-project", "test-topic").FullName());
    auto status = stub.CreateTopic(context, topic);
    EXPECT_STATUS_OK(status);
  }
}

TEST_F(PublisherMetadataTest, GetTopic) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, GetTopic)
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::GetTopicRequest const& request) {
        IsContextMDValid(context, "google.pubsub.v1.Publisher.GetTopic",
                         request);
        return make_status_or(google::pubsub::v1::Topic{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::GetTopicRequest const&) {
        ValidateNoUserProject(context);
        return make_status_or(google::pubsub::v1::Topic{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::GetTopicRequest const&) {
        ValidateTestUserProject(context);
        return make_status_or(google::pubsub::v1::Topic{});
      });

  PublisherMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    grpc::ClientContext context;
    google::pubsub::v1::GetTopicRequest request;
    request.set_topic(pubsub::Topic("test-project", "test-topic").FullName());
    auto status = stub.GetTopic(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST_F(PublisherMetadataTest, UpdateTopic) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, UpdateTopic)
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::UpdateTopicRequest const& request) {
        IsContextMDValid(context, "google.pubsub.v1.Publisher.UpdateTopic",
                         request);
        return make_status_or(google::pubsub::v1::Topic{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::UpdateTopicRequest const&) {
        ValidateNoUserProject(context);
        return make_status_or(google::pubsub::v1::Topic{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::UpdateTopicRequest const&) {
        ValidateTestUserProject(context);
        return make_status_or(google::pubsub::v1::Topic{});
      });

  PublisherMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    grpc::ClientContext context;
    google::pubsub::v1::UpdateTopicRequest request;
    request.mutable_topic()->set_name(
        pubsub::Topic("test-project", "test-topic").FullName());
    auto status = stub.UpdateTopic(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST_F(PublisherMetadataTest, ListTopics) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, ListTopics)
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::ListTopicsRequest const& request) {
        IsContextMDValid(context, "google.pubsub.v1.Publisher.ListTopics",
                         request);
        return make_status_or(google::pubsub::v1::ListTopicsResponse{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::ListTopicsRequest const&) {
        ValidateNoUserProject(context);
        return make_status_or(google::pubsub::v1::ListTopicsResponse{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::ListTopicsRequest const&) {
        ValidateTestUserProject(context);
        return make_status_or(google::pubsub::v1::ListTopicsResponse{});
      });

  PublisherMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    grpc::ClientContext context;
    google::pubsub::v1::ListTopicsRequest request;
    request.set_project("projects/test-project");
    auto status = stub.ListTopics(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST_F(PublisherMetadataTest, DeleteTopic) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, DeleteTopic)
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::DeleteTopicRequest const& request) {
        IsContextMDValid(context, "google.pubsub.v1.Publisher.DeleteTopic",
                         request);
        return Status{};
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::DeleteTopicRequest const&) {
        ValidateNoUserProject(context);
        return Status{};
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::DeleteTopicRequest const&) {
        ValidateTestUserProject(context);
        return Status{};
      });

  PublisherMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    grpc::ClientContext context;
    google::pubsub::v1::DeleteTopicRequest request;
    request.set_topic(pubsub::Topic("test-project", "test-topic").FullName());
    auto status = stub.DeleteTopic(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST_F(PublisherMetadataTest, DetachSubscription) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, DetachSubscription)
      .WillOnce(
          [this](grpc::ClientContext& context,
                 google::pubsub::v1::DetachSubscriptionRequest const& request) {
            IsContextMDValid(context,
                             "google.pubsub.v1.Publisher.DetachSubscription",
                             request);
            return make_status_or(
                google::pubsub::v1::DetachSubscriptionResponse{});
          })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::DetachSubscriptionRequest const&) {
        ValidateNoUserProject(context);
        return make_status_or(google::pubsub::v1::DetachSubscriptionResponse{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::DetachSubscriptionRequest const&) {
        ValidateTestUserProject(context);
        return make_status_or(google::pubsub::v1::DetachSubscriptionResponse{});
      });

  PublisherMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    grpc::ClientContext context;
    google::pubsub::v1::DetachSubscriptionRequest request;
    request.set_subscription(
        pubsub::Subscription("test-project", "test-subscription").FullName());
    auto status = stub.DetachSubscription(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST_F(PublisherMetadataTest, ListTopicSubscriptions) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, ListTopicSubscriptions)
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::ListTopicSubscriptionsRequest const&
                           request) {
        IsContextMDValid(context,
                         "google.pubsub.v1.Publisher.ListTopicSubscriptions",
                         request);
        return make_status_or(
            google::pubsub::v1::ListTopicSubscriptionsResponse{});
      })
      .WillOnce(
          [this](grpc::ClientContext& context,
                 google::pubsub::v1::ListTopicSubscriptionsRequest const&) {
            ValidateNoUserProject(context);
            return make_status_or(
                google::pubsub::v1::ListTopicSubscriptionsResponse{});
          })
      .WillOnce(
          [this](grpc::ClientContext& context,
                 google::pubsub::v1::ListTopicSubscriptionsRequest const&) {
            ValidateTestUserProject(context);
            return make_status_or(
                google::pubsub::v1::ListTopicSubscriptionsResponse{});
          });

  PublisherMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    grpc::ClientContext context;
    google::pubsub::v1::ListTopicSubscriptionsRequest request;
    request.set_topic(pubsub::Topic("test-project", "test-topic").FullName());
    auto status = stub.ListTopicSubscriptions(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST_F(PublisherMetadataTest, ListTopicSnapshots) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, ListTopicSnapshots)
      .WillOnce(
          [this](grpc::ClientContext& context,
                 google::pubsub::v1::ListTopicSnapshotsRequest const& request) {
            IsContextMDValid(context,
                             "google.pubsub.v1.Publisher.ListTopicSnapshots",
                             request);
            return make_status_or(
                google::pubsub::v1::ListTopicSnapshotsResponse{});
          })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::ListTopicSnapshotsRequest const&) {
        ValidateNoUserProject(context);
        return make_status_or(google::pubsub::v1::ListTopicSnapshotsResponse{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::ListTopicSnapshotsRequest const&) {
        ValidateTestUserProject(context);
        return make_status_or(google::pubsub::v1::ListTopicSnapshotsResponse{});
      });

  PublisherMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    grpc::ClientContext context;
    google::pubsub::v1::ListTopicSnapshotsRequest request;
    request.set_topic(pubsub::Topic("test-project", "test-topic").FullName());
    auto status = stub.ListTopicSnapshots(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST_F(PublisherMetadataTest, AsyncPublish) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([this](google::cloud::CompletionQueue&,
                       std::unique_ptr<grpc::ClientContext> context,
                       google::pubsub::v1::PublishRequest const& request) {
        IsContextMDValid(*context, "google.pubsub.v1.Publisher.Publish",
                         request);
        return make_ready_future(
            make_status_or(google::pubsub::v1::PublishResponse{}));
      })
      .WillOnce([this](google::cloud::CompletionQueue&,
                       std::unique_ptr<grpc::ClientContext> context,
                       google::pubsub::v1::PublishRequest const&) {
        ValidateNoUserProject(*context);
        return make_ready_future(
            make_status_or(google::pubsub::v1::PublishResponse{}));
      })
      .WillOnce([this](google::cloud::CompletionQueue&,
                       std::unique_ptr<grpc::ClientContext> context,
                       google::pubsub::v1::PublishRequest const&) {
        ValidateTestUserProject(*context);
        return make_ready_future(
            make_status_or(google::pubsub::v1::PublishResponse{}));
      });

  PublisherMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    google::cloud::CompletionQueue cq;
    google::pubsub::v1::PublishRequest request;
    request.set_topic(pubsub::Topic("test-project", "test-topic").FullName());
    auto status =
        stub.AsyncPublish(cq, absl::make_unique<grpc::ClientContext>(), request)
            .get();
    EXPECT_STATUS_OK(status);
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
