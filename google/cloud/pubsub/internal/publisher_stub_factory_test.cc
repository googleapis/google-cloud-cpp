// Copyright 2023 Google LLC
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

#include "google/cloud/pubsub/internal/publisher_stub_factory.h"
#include "google/cloud/pubsub/testing/mock_publisher_stub.h"
#include "google/cloud/credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include "google/cloud/testing_util/validate_propagator.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::pubsub_testing::MockPublisherStub;
using ::google::cloud::testing_util::ScopedLog;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::NotNull;

using BasePublisherStubFactory = std::function<std::shared_ptr<PublisherStub>(
    std::shared_ptr<grpc::Channel>)>;

// The point of these tests is to verify that the `CreatePublisherStub` factory
// function injects the right decorators. We do this by observing the
// side effects of these decorators. All the tests have nearly identical
// structure. They create a fully decorated stub, configured to round-robin
// over kTestChannel mocks.  The first mock expects a call, the remaining mocks
// expect no calls.  Some of these side effects can only be verified as part of
// the first mock.

class PublisherStubFactory : public ::testing::Test {
 protected:
  void IsContextMDValid(grpc::ClientContext& context, std::string const& method,
                        google::protobuf::Message const& request) {
    validate_metadata_fixture_.IsContextMDValid(
        context, method, request,
        google::cloud::internal::HandCraftedLibClientHeader());
  }

 private:
  ValidateMetadataFixture validate_metadata_fixture_;
};

auto constexpr kTestChannels = 3;
using MockFactory = ::testing::MockFunction<std::shared_ptr<PublisherStub>(
    std::shared_ptr<grpc::Channel>)>;
static_assert(std::is_same<decltype(MockFactory{}.AsStdFunction()),
                           BasePublisherStubFactory>::value,
              "Mismatched mock factory type");
std::shared_ptr<PublisherStub> CreateTestStub(
    CompletionQueue cq, BasePublisherStubFactory const& factory) {
  auto credentials = google::cloud::MakeAccessTokenCredentials(
      "test-only-invalid",
      std::chrono::system_clock::now() + std::chrono::minutes(5));
  return CreateDecoratedStubs(std::move(cq),
                              google::cloud::Options{}
                                  .set<GrpcNumChannelsOption>(kTestChannels)
                                  .set<TracingComponentsOption>({"rpc"})
                                  .set<UnifiedCredentialsOption>(credentials),
                              factory);
}

// The following unit tests are verifying the corresponding decorators are
// added. They all use the same CreateTopic rpc.
TEST_F(PublisherStubFactory, RoundRobin) {
  ::testing::InSequence sequence;
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockPublisherStub>();
        EXPECT_CALL(*mock, CreateTopic)
            .WillOnce([](grpc::ClientContext&, Options const&,
                         google::pubsub::v1::Topic const&) {
              return StatusOr<google::pubsub::v1::Topic>(
                  Status(StatusCode::kUnavailable, "nothing here"));
            });
        return mock;
      });
  // Verify the round robin decorator is present.
  EXPECT_CALL(factory, Call)
      .Times(kTestChannels - 1)
      .WillRepeatedly([](std::shared_ptr<grpc::Channel> const&) {
        return std::make_shared<MockPublisherStub>();
      });

  CompletionQueue cq;
  grpc::ClientContext context;
  google::pubsub::v1::Topic req;
  req.set_name("projects/test-project/topics/my-topic");
  auto stub = CreateTestStub(cq, factory.AsStdFunction());
  auto response = stub->CreateTopic(context, Options{}, req);
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

TEST_F(PublisherStubFactory, Auth) {
  ::testing::InSequence sequence;
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockPublisherStub>();
        EXPECT_CALL(*mock, CreateTopic)
            .WillOnce([](grpc::ClientContext& context, Options const&,
                         google::pubsub::v1::Topic const&) {
              // Verify the Auth decorator is present
              EXPECT_THAT(context.credentials(), NotNull());
              return StatusOr<google::pubsub::v1::Topic>(
                  Status(StatusCode::kUnavailable, "nothing here"));
            });
        return mock;
      });
  EXPECT_CALL(factory, Call)
      .WillRepeatedly([](std::shared_ptr<grpc::Channel> const&) {
        return std::make_shared<MockPublisherStub>();
      });

  CompletionQueue cq;
  grpc::ClientContext context;
  google::pubsub::v1::Topic req;
  req.set_name("projects/test-project/topics/my-topic");
  auto stub = CreateTestStub(cq, factory.AsStdFunction());
  auto response = stub->CreateTopic(context, Options{}, req);
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

TEST_F(PublisherStubFactory, Metadata) {
  ::testing::InSequence sequence;
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([this](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockPublisherStub>();
        EXPECT_CALL(*mock, CreateTopic)
            .WillOnce([this](grpc::ClientContext& context, Options const&,
                             google::pubsub::v1::Topic const& request) {
              // Verify the Metadata decorator is present
              IsContextMDValid(
                  context, "google.pubsub.v1.Publisher.CreateTopic", request);
              return StatusOr<google::pubsub::v1::Topic>(
                  Status(StatusCode::kUnavailable, "nothing here"));
            });
        return mock;
      });
  EXPECT_CALL(factory, Call)
      .WillRepeatedly([](std::shared_ptr<grpc::Channel> const&) {
        return std::make_shared<MockPublisherStub>();
      });

  CompletionQueue cq;
  grpc::ClientContext context;
  google::pubsub::v1::Topic req;
  req.set_name("projects/test-project/topics/my-topic");
  auto stub = CreateTestStub(cq, factory.AsStdFunction());
  auto response = stub->CreateTopic(context, Options{}, req);
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

TEST_F(PublisherStubFactory, Logging) {
  ::testing::InSequence sequence;
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockPublisherStub>();
        EXPECT_CALL(*mock, CreateTopic)
            .WillOnce([](grpc::ClientContext&, Options const&,
                         google::pubsub::v1::Topic const&) {
              return StatusOr<google::pubsub::v1::Topic>(
                  Status(StatusCode::kUnavailable, "nothing here"));
            });
        return mock;
      });
  EXPECT_CALL(factory, Call)
      .WillRepeatedly([](std::shared_ptr<grpc::Channel> const&) {
        return std::make_shared<MockPublisherStub>();
      });

  ScopedLog log;
  CompletionQueue cq;
  grpc::ClientContext context;
  google::pubsub::v1::Topic req;
  req.set_name("projects/test-project/topics/my-topic");
  auto stub = CreateTestStub(cq, factory.AsStdFunction());
  auto response = stub->CreateTopic(context, Options{}, req);
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
  // Verify the logging decorator is present.
  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("CreateTopic")));
}

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
using ::google::cloud::testing_util::DisableTracing;
using ::google::cloud::testing_util::EnableTracing;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::ValidateNoPropagator;
using ::google::cloud::testing_util::ValidatePropagator;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

TEST_F(PublisherStubFactory, TracingEnabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockPublisherStub>();
        EXPECT_CALL(*mock, CreateTopic)
            .WillOnce([](grpc::ClientContext& context, Options const&,
                         google::pubsub::v1::Topic const&) {
              ValidatePropagator(context);
              return StatusOr<google::pubsub::v1::Topic>(
                  Status(StatusCode::kUnavailable, "nothing here"));
            });
        return mock;
      });

  CompletionQueue cq;
  grpc::ClientContext context;
  google::pubsub::v1::Topic req;
  auto stub = CreateDecoratedStubs(
      std::move(cq),
      EnableTracing(
          Options{}
              .set<EndpointOption>("localhost:1")
              .set<GrpcNumChannelsOption>(1)
              .set<UnifiedCredentialsOption>(MakeInsecureCredentials())),
      factory.AsStdFunction());
  req.set_name("projects/test-project/topics/my-topic");
  auto response = stub->CreateTopic(context, Options{}, req);
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));

  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(SpanNamed("google.pubsub.v1.Publisher/CreateTopic")));
}

TEST_F(PublisherStubFactory, TracingDisabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockPublisherStub>();
        EXPECT_CALL(*mock, CreateTopic)
            .WillOnce([](grpc::ClientContext& context, Options const&,
                         google::pubsub::v1::Topic const&) {
              ValidateNoPropagator(context);
              return StatusOr<google::pubsub::v1::Topic>(
                  Status(StatusCode::kUnavailable, "nothing here"));
            });
        return mock;
      });

  CompletionQueue cq;
  grpc::ClientContext context;
  google::pubsub::v1::Topic req;
  auto stub = CreateDecoratedStubs(
      std::move(cq),
      DisableTracing(
          Options{}
              .set<EndpointOption>("localhost:1")
              .set<GrpcNumChannelsOption>(1)
              .set<UnifiedCredentialsOption>(MakeInsecureCredentials())),
      factory.AsStdFunction());
  req.set_name("projects/test-project/topics/my-topic");
  auto response = stub->CreateTopic(context, Options{}, req);
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));

  EXPECT_THAT(span_catcher->GetSpans(), IsEmpty());
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

// The following tests are for all the rpcs on the stub.
TEST_F(PublisherStubFactory, CreateTopic) {
  ::testing::InSequence sequence;
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([this](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockPublisherStub>();
        EXPECT_CALL(*mock, CreateTopic)
            .WillOnce([this](grpc::ClientContext& context, Options const&,
                             google::pubsub::v1::Topic const& request) {
              // Verify the Auth decorator is present
              EXPECT_THAT(context.credentials(), NotNull());
              // Verify the Metadata decorator is present
              IsContextMDValid(
                  context, "google.pubsub.v1.Publisher.CreateTopic", request);
              return StatusOr<google::pubsub::v1::Topic>(
                  Status(StatusCode::kUnavailable, "nothing here"));
            });
        return mock;
      });
  EXPECT_CALL(factory, Call)
      .Times(kTestChannels - 1)
      .WillRepeatedly([](std::shared_ptr<grpc::Channel> const&) {
        return std::make_shared<MockPublisherStub>();
      });

  ScopedLog log;
  CompletionQueue cq;
  grpc::ClientContext context;
  google::pubsub::v1::Topic req;
  req.set_name("projects/test-project/topics/my-topic");
  auto stub = CreateTestStub(cq, factory.AsStdFunction());
  auto response = stub->CreateTopic(context, Options{}, req);
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("CreateTopic")));
}

TEST_F(PublisherStubFactory, UpdateTopic) {
  ::testing::InSequence sequence;
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([this](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockPublisherStub>();
        EXPECT_CALL(*mock, UpdateTopic)
            .WillOnce(
                [this](grpc::ClientContext& context, Options const&,
                       google::pubsub::v1::UpdateTopicRequest const& request) {
                  // Verify the Auth decorator is present
                  EXPECT_THAT(context.credentials(), NotNull());
                  // Verify the Metadata decorator is present
                  IsContextMDValid(context,
                                   "google.pubsub.v1.Publisher.UpdateTopic",
                                   request);
                  return StatusOr<google::pubsub::v1::Topic>(
                      Status(StatusCode::kUnavailable, "nothing here"));
                });
        return mock;
      });
  EXPECT_CALL(factory, Call)
      .Times(kTestChannels - 1)
      .WillRepeatedly([](std::shared_ptr<grpc::Channel> const&) {
        return std::make_shared<MockPublisherStub>();
      });

  ScopedLog log;
  CompletionQueue cq;
  grpc::ClientContext context;
  google::pubsub::v1::UpdateTopicRequest req;
  req.mutable_topic()->set_name("projects/test-project/topics/my-topic");
  auto stub = CreateTestStub(cq, factory.AsStdFunction());
  auto response = stub->UpdateTopic(context, Options{}, req);
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("UpdateTopic")));
}

TEST_F(PublisherStubFactory, Publish) {
  ::testing::InSequence sequence;
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([this](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockPublisherStub>();
        EXPECT_CALL(*mock, Publish)
            .WillOnce(
                [this](grpc::ClientContext& context, Options const&,
                       google::pubsub::v1::PublishRequest const& request) {
                  // Verify the Auth decorator is present
                  EXPECT_THAT(context.credentials(), NotNull());
                  // Verify the Metadata decorator is present
                  IsContextMDValid(
                      context, "google.pubsub.v1.Publisher.Publish", request);
                  return StatusOr<google::pubsub::v1::PublishResponse>(
                      Status(StatusCode::kUnavailable, "nothing here"));
                });
        return mock;
      });
  EXPECT_CALL(factory, Call)
      .Times(kTestChannels - 1)
      .WillRepeatedly([](std::shared_ptr<grpc::Channel> const&) {
        return std::make_shared<MockPublisherStub>();
      });

  ScopedLog log;
  CompletionQueue cq;
  grpc::ClientContext context;
  google::pubsub::v1::PublishRequest req;
  req.set_topic("projects/test-project/topics/my-topic");
  auto stub = CreateTestStub(cq, factory.AsStdFunction());
  auto response = stub->Publish(context, Options{}, req);
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("Publish")));
}

TEST_F(PublisherStubFactory, GetTopic) {
  ::testing::InSequence sequence;
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([this](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockPublisherStub>();
        EXPECT_CALL(*mock, GetTopic)
            .WillOnce(
                [this](grpc::ClientContext& context, Options const&,
                       google::pubsub::v1::GetTopicRequest const& request) {
                  // Verify the Auth decorator is present
                  EXPECT_THAT(context.credentials(), NotNull());
                  // Verify the Metadata decorator is present
                  IsContextMDValid(
                      context, "google.pubsub.v1.Publisher.GetTopic", request);
                  return StatusOr<google::pubsub::v1::Topic>(
                      Status(StatusCode::kUnavailable, "nothing here"));
                });
        return mock;
      });
  EXPECT_CALL(factory, Call)
      .Times(kTestChannels - 1)
      .WillRepeatedly([](std::shared_ptr<grpc::Channel> const&) {
        return std::make_shared<MockPublisherStub>();
      });

  ScopedLog log;
  CompletionQueue cq;
  grpc::ClientContext context;
  google::pubsub::v1::GetTopicRequest req;
  req.set_topic("projects/test-project/topics/my-topic");
  auto stub = CreateTestStub(cq, factory.AsStdFunction());
  auto response = stub->GetTopic(context, Options{}, req);
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("GetTopic")));
}

TEST_F(PublisherStubFactory, ListTopics) {
  ::testing::InSequence sequence;
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([this](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockPublisherStub>();
        EXPECT_CALL(*mock, ListTopics)
            .WillOnce(
                [this](grpc::ClientContext& context, Options const&,
                       google::pubsub::v1::ListTopicsRequest const& request) {
                  // Verify the Auth decorator is present
                  EXPECT_THAT(context.credentials(), NotNull());
                  // Verify the Metadata decorator is present
                  IsContextMDValid(context,
                                   "google.pubsub.v1.Publisher.ListTopics",
                                   request);
                  return StatusOr<google::pubsub::v1::ListTopicsResponse>(
                      Status(StatusCode::kUnavailable, "nothing here"));
                });
        return mock;
      });
  EXPECT_CALL(factory, Call)
      .Times(kTestChannels - 1)
      .WillRepeatedly([](std::shared_ptr<grpc::Channel> const&) {
        return std::make_shared<MockPublisherStub>();
      });

  ScopedLog log;
  CompletionQueue cq;
  grpc::ClientContext context;
  google::pubsub::v1::ListTopicsRequest req;
  req.set_project("projects/test-project");
  auto stub = CreateTestStub(cq, factory.AsStdFunction());
  auto response = stub->ListTopics(context, Options{}, req);
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("ListTopics")));
}

TEST_F(PublisherStubFactory, ListTopicSubscriptions) {
  ::testing::InSequence sequence;
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([this](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockPublisherStub>();
        EXPECT_CALL(*mock, ListTopicSubscriptions)
            .WillOnce(
                [this](grpc::ClientContext& context, Options const&,
                       google::pubsub::v1::ListTopicSubscriptionsRequest const&
                           request) {
                  // Verify the Auth decorator is present
                  EXPECT_THAT(context.credentials(), NotNull());
                  // Verify the Metadata decorator is present
                  IsContextMDValid(
                      context,
                      "google.pubsub.v1.Publisher.ListTopicSubscriptions",
                      request);
                  return StatusOr<
                      google::pubsub::v1::ListTopicSubscriptionsResponse>(
                      Status(StatusCode::kUnavailable, "nothing here"));
                });
        return mock;
      });
  EXPECT_CALL(factory, Call)
      .Times(kTestChannels - 1)
      .WillRepeatedly([](std::shared_ptr<grpc::Channel> const&) {
        return std::make_shared<MockPublisherStub>();
      });

  ScopedLog log;
  CompletionQueue cq;
  grpc::ClientContext context;
  google::pubsub::v1::ListTopicSubscriptionsRequest req;
  req.set_topic("projects/test-project/topics/my-topic");
  auto stub = CreateTestStub(cq, factory.AsStdFunction());
  auto response = stub->ListTopicSubscriptions(context, Options{}, req);
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
  EXPECT_THAT(log.ExtractLines(),
              Contains(HasSubstr("ListTopicSubscriptions")));
}

TEST_F(PublisherStubFactory, ListTopicSnapshots) {
  ::testing::InSequence sequence;
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([this](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockPublisherStub>();
        EXPECT_CALL(*mock, ListTopicSnapshots)
            .WillOnce([this](
                          grpc::ClientContext& context, Options const&,
                          google::pubsub::v1::ListTopicSnapshotsRequest const&
                              request) {
              // Verify the Auth decorator is present
              EXPECT_THAT(context.credentials(), NotNull());
              // Verify the Metadata decorator is present
              IsContextMDValid(context,
                               "google.pubsub.v1.Publisher.ListTopicSnapshots",
                               request);
              return StatusOr<google::pubsub::v1::ListTopicSnapshotsResponse>(
                  Status(StatusCode::kUnavailable, "nothing here"));
            });
        return mock;
      });
  EXPECT_CALL(factory, Call)
      .Times(kTestChannels - 1)
      .WillRepeatedly([](std::shared_ptr<grpc::Channel> const&) {
        return std::make_shared<MockPublisherStub>();
      });

  ScopedLog log;
  CompletionQueue cq;
  grpc::ClientContext context;
  google::pubsub::v1::ListTopicSnapshotsRequest req;
  req.set_topic("projects/test-project/topics/my-topic");
  auto stub = CreateTestStub(cq, factory.AsStdFunction());
  auto response = stub->ListTopicSnapshots(context, Options{}, req);
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("ListTopicSnapshots")));
}

TEST_F(PublisherStubFactory, DeleteTopic) {
  ::testing::InSequence sequence;
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([this](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockPublisherStub>();
        EXPECT_CALL(*mock, DeleteTopic)
            .WillOnce(
                [this](grpc::ClientContext& context, Options const&,
                       google::pubsub::v1::DeleteTopicRequest const& request) {
                  // Verify the Auth decorator is present
                  EXPECT_THAT(context.credentials(), NotNull());
                  // Verify the Metadata decorator is present
                  IsContextMDValid(context,
                                   "google.pubsub.v1.Publisher.DeleteTopic",
                                   request);
                  return Status(StatusCode::kUnavailable, "nothing here");
                });
        return mock;
      });
  EXPECT_CALL(factory, Call)
      .Times(kTestChannels - 1)
      .WillRepeatedly([](std::shared_ptr<grpc::Channel> const&) {
        return std::make_shared<MockPublisherStub>();
      });

  ScopedLog log;
  CompletionQueue cq;
  grpc::ClientContext context;
  google::pubsub::v1::DeleteTopicRequest req;
  req.set_topic("projects/test-project/topics/my-topic");
  auto stub = CreateTestStub(cq, factory.AsStdFunction());
  auto response = stub->DeleteTopic(context, Options{}, req);
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("DeleteTopic")));
}

TEST_F(PublisherStubFactory, DetachSubscription) {
  ::testing::InSequence sequence;
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([this](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockPublisherStub>();
        EXPECT_CALL(*mock, DetachSubscription)
            .WillOnce([this](
                          grpc::ClientContext& context, Options const&,
                          google::pubsub::v1::DetachSubscriptionRequest const&
                              request) {
              // Verify the Auth decorator is present
              EXPECT_THAT(context.credentials(), NotNull());
              // Verify the Metadata decorator is present
              IsContextMDValid(context,
                               "google.pubsub.v1.Publisher.DetachSubscription",
                               request);
              return StatusOr<google::pubsub::v1::DetachSubscriptionResponse>(
                  Status(StatusCode::kUnavailable, "nothing here"));
            });
        return mock;
      });
  EXPECT_CALL(factory, Call)
      .Times(kTestChannels - 1)
      .WillRepeatedly([](std::shared_ptr<grpc::Channel> const&) {
        return std::make_shared<MockPublisherStub>();
      });

  ScopedLog log;
  CompletionQueue cq;
  grpc::ClientContext context;
  google::pubsub::v1::DetachSubscriptionRequest req;
  req.set_subscription("projects/test-project/subscriptions/my-topic");
  auto stub = CreateTestStub(cq, factory.AsStdFunction());
  auto response = stub->DetachSubscription(context, Options{}, req);
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("DetachSubscription")));
}

TEST_F(PublisherStubFactory, AsyncPublish) {
  ::testing::InSequence sequence;
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([this](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockPublisherStub>();
        EXPECT_CALL(*mock, AsyncPublish)
            .WillOnce(
                [this](google::cloud::CompletionQueue&, auto context,
                       google::cloud::internal::ImmutableOptions const& options,
                       google::pubsub::v1::PublishRequest const& request) {
                  // Verify the Auth decorator is present
                  EXPECT_THAT(context->credentials(), NotNull());
                  // Verify the Metadata decorator is present
                  IsContextMDValid(
                      *context, "google.pubsub.v1.Publisher.Publish", request);
                  // Verify the options are carried forward
                  EXPECT_EQ(options->get<UserProjectOption>(),
                            "test-user-project");
                  return make_ready_future(
                      StatusOr<google::pubsub::v1::PublishResponse>(
                          Status(StatusCode::kUnavailable, "nothing here")));
                });
        return mock;
      });
  EXPECT_CALL(factory, Call)
      .Times(kTestChannels - 1)
      .WillRepeatedly([](std::shared_ptr<grpc::Channel> const&) {
        return std::make_shared<MockPublisherStub>();
      });

  ScopedLog log;
  CompletionQueue cq;
  google::pubsub::v1::PublishRequest req;
  req.set_topic("projects/test-project/topics/my-topic");
  auto stub = CreateTestStub(cq, factory.AsStdFunction());
  auto response = stub->AsyncPublish(
      cq, std::make_shared<grpc::ClientContext>(),
      internal::MakeImmutableOptions(
          Options{}.set<UserProjectOption>("test-user-project")),
      req);
  EXPECT_THAT(response.get(), StatusIs(StatusCode::kUnavailable));
  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("AsyncPublish")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
