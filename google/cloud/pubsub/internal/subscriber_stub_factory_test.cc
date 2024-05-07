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

#include "google/cloud/pubsub/internal/subscriber_stub_factory.h"
#include "google/cloud/pubsub/testing/mock_subscriber_stub.h"
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

using ::google::cloud::pubsub_testing::MockSubscriberStub;
using ::google::cloud::testing_util::ScopedLog;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::NotNull;

using BaseSubscriberStubFactory = std::function<std::shared_ptr<SubscriberStub>(
    std::shared_ptr<grpc::Channel>)>;

// The point of these tests is to verify that the `CreateSubscriberStub` factory
// function injects the right decorators. We do this by observing the
// side effects of these decorators. All the tests have nearly identical
// structure. They create a fully decorated stub, configured to round-robin
// over kTestChannel mocks.  The first mock expects a call, the remaining mocks
// expect no calls.  Some of these side effects can only be verified as part of
// the first mock.

class SubscriberStubFactory : public ::testing::Test {
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
using MockFactory = ::testing::MockFunction<std::shared_ptr<SubscriberStub>(
    std::shared_ptr<grpc::Channel>)>;
static_assert(std::is_same<decltype(MockFactory{}.AsStdFunction()),
                           BaseSubscriberStubFactory>::value,
              "Mismatched mock factory type");
std::shared_ptr<SubscriberStub> CreateTestStub(
    CompletionQueue cq, BaseSubscriberStubFactory const& factory) {
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
// added. They all use the same CreateSubscription rpc.
TEST_F(SubscriberStubFactory, RoundRobin) {
  ::testing::InSequence sequence;
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockSubscriberStub>();
        EXPECT_CALL(*mock, CreateSubscription)
            .WillOnce([](grpc::ClientContext&, Options const&,
                         google::pubsub::v1::Subscription const&) {
              return StatusOr<google::pubsub::v1::Subscription>(
                  Status(StatusCode::kUnavailable, "nothing here"));
            });
        return mock;
      });
  // Verify the round robin decorator is present.
  EXPECT_CALL(factory, Call)
      .Times(kTestChannels - 1)
      .WillRepeatedly([](std::shared_ptr<grpc::Channel> const&) {
        return std::make_shared<MockSubscriberStub>();
      });

  CompletionQueue cq;
  grpc::ClientContext context;
  google::pubsub::v1::Subscription req;
  req.set_name("projects/test-project/subscriptions/my-sub");
  auto stub = CreateTestStub(cq, factory.AsStdFunction());
  auto response = stub->CreateSubscription(context, Options{}, req);
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

TEST_F(SubscriberStubFactory, Auth) {
  ::testing::InSequence sequence;
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockSubscriberStub>();
        EXPECT_CALL(*mock, CreateSubscription)
            .WillOnce([](grpc::ClientContext& context, Options const&,
                         google::pubsub::v1::Subscription const&) {
              // Verify the Auth decorator is present
              EXPECT_THAT(context.credentials(), NotNull());
              return StatusOr<google::pubsub::v1::Subscription>(
                  Status(StatusCode::kUnavailable, "nothing here"));
            });
        return mock;
      });
  EXPECT_CALL(factory, Call)
      .WillRepeatedly([](std::shared_ptr<grpc::Channel> const&) {
        return std::make_shared<MockSubscriberStub>();
      });

  CompletionQueue cq;
  grpc::ClientContext context;
  google::pubsub::v1::Subscription req;
  req.set_name("projects/test-project/subscriptions/my-sub");
  auto stub = CreateTestStub(cq, factory.AsStdFunction());
  auto response = stub->CreateSubscription(context, Options{}, req);
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

TEST_F(SubscriberStubFactory, Metadata) {
  ::testing::InSequence sequence;
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([this](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockSubscriberStub>();
        EXPECT_CALL(*mock, CreateSubscription)
            .WillOnce([this](grpc::ClientContext& context, Options const&,
                             google::pubsub::v1::Subscription const& request) {
              // Verify the Metadata decorator is present
              IsContextMDValid(context,
                               "google.pubsub.v1.Subscriber.CreateSubscription",
                               request);
              return StatusOr<google::pubsub::v1::Subscription>(
                  Status(StatusCode::kUnavailable, "nothing here"));
            });
        return mock;
      });
  EXPECT_CALL(factory, Call)
      .WillRepeatedly([](std::shared_ptr<grpc::Channel> const&) {
        return std::make_shared<MockSubscriberStub>();
      });

  CompletionQueue cq;
  grpc::ClientContext context;
  google::pubsub::v1::Subscription req;
  req.set_name("projects/test-project/subscriptions/my-sub");
  auto stub = CreateTestStub(cq, factory.AsStdFunction());
  auto response = stub->CreateSubscription(context, Options{}, req);
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

TEST_F(SubscriberStubFactory, Logging) {
  ::testing::InSequence sequence;
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockSubscriberStub>();
        EXPECT_CALL(*mock, CreateSubscription)
            .WillOnce([](grpc::ClientContext&, Options const&,
                         google::pubsub::v1::Subscription const&) {
              return StatusOr<google::pubsub::v1::Subscription>(
                  Status(StatusCode::kUnavailable, "nothing here"));
            });
        return mock;
      });
  EXPECT_CALL(factory, Call)
      .WillRepeatedly([](std::shared_ptr<grpc::Channel> const&) {
        return std::make_shared<MockSubscriberStub>();
      });

  ScopedLog log;
  CompletionQueue cq;
  grpc::ClientContext context;
  google::pubsub::v1::Subscription req;
  req.set_name("projects/test-project/subscriptions/my-sub");
  auto stub = CreateTestStub(cq, factory.AsStdFunction());
  auto response = stub->CreateSubscription(context, Options{}, req);
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
  // Verify the logging decorator is present.
  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("CreateSubscription")));
}

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
using ::google::cloud::testing_util::DisableTracing;
using ::google::cloud::testing_util::EnableTracing;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::ValidateNoPropagator;
using ::google::cloud::testing_util::ValidatePropagator;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

TEST_F(SubscriberStubFactory, TracingEnabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockSubscriberStub>();
        EXPECT_CALL(*mock, CreateSubscription)
            .WillOnce([](grpc::ClientContext& context, Options const&,
                         google::pubsub::v1::Subscription const&) {
              ValidatePropagator(context);
              return StatusOr<google::pubsub::v1::Subscription>(
                  Status(StatusCode::kUnavailable, "nothing here"));
            });
        return mock;
      });

  CompletionQueue cq;
  grpc::ClientContext context;
  google::pubsub::v1::Subscription req;
  auto stub = CreateDecoratedStubs(
      std::move(cq),
      EnableTracing(
          Options{}
              .set<EndpointOption>("localhost:1")
              .set<GrpcNumChannelsOption>(1)
              .set<UnifiedCredentialsOption>(MakeInsecureCredentials())),
      factory.AsStdFunction());
  req.set_name("projects/test-project/topics/my-subscription");
  auto response = stub->CreateSubscription(context, Options{}, req);
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));

  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(SpanNamed("google.pubsub.v1.Subscriber/CreateSubscription")));
}

TEST_F(SubscriberStubFactory, TracingDisabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockSubscriberStub>();
        EXPECT_CALL(*mock, CreateSubscription)
            .WillOnce([](grpc::ClientContext& context, Options const&,
                         google::pubsub::v1::Subscription const&) {
              ValidateNoPropagator(context);
              return StatusOr<google::pubsub::v1::Subscription>(
                  Status(StatusCode::kUnavailable, "nothing here"));
            });
        return mock;
      });

  CompletionQueue cq;
  grpc::ClientContext context;
  google::pubsub::v1::Subscription req;
  auto stub = CreateDecoratedStubs(
      std::move(cq),
      DisableTracing(
          Options{}
              .set<EndpointOption>("localhost:1")
              .set<GrpcNumChannelsOption>(1)
              .set<UnifiedCredentialsOption>(MakeInsecureCredentials())),
      factory.AsStdFunction());
  req.set_name("projects/test-project/topics/my-subscription");
  auto response = stub->CreateSubscription(context, Options{}, req);
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));

  EXPECT_THAT(span_catcher->GetSpans(), IsEmpty());
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
