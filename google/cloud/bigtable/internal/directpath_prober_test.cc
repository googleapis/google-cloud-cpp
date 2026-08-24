// Copyright 2026 Google LLC
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

#include "google/cloud/bigtable/internal/directpath_prober.h"
#include "google/cloud/bigtable/instance_resource.h"
#include "google/cloud/bigtable/internal/data_connection_impl.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/project.h"
#include "google/cloud/testing_util/mock_grpc_authentication_strategy.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/bigtable/v2/bigtable.grpc.pb.h>
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::MockAuthenticationStrategy;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::ExplainMatchResult;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;

MATCHER_P(ProbeSuccess, matcher, "") {
  return ExplainMatchResult(matcher, arg.success, result_listener);
}

MATCHER_P(ProbeIpPreference, matcher, "") {
  return ExplainMatchResult(matcher, arg.ip_preference, result_listener);
}

MATCHER_P(ProbePeerAddress, matcher, "") {
  return ExplainMatchResult(matcher, arg.peer_address, result_listener);
}

class FakeBigtableService final
    : public google::bigtable::v2::Bigtable::Service {
 public:
  grpc::Status PingAndWarm(
      grpc::ServerContext* /*context*/,
      google::bigtable::v2::PingAndWarmRequest const* request,
      google::bigtable::v2::PingAndWarmResponse* /*response*/) override {
    last_request = *request;
    return ping_status;
  }

  google::bigtable::v2::PingAndWarmRequest last_request;
  grpc::Status ping_status = grpc::Status::OK;
};

TEST(DirectPathProberTest, ToStringIpPreference) {
  EXPECT_THAT(ToString(IpPreference::kIpv4), Eq("ipv4"));
  EXPECT_THAT(ToString(IpPreference::kIpv6), Eq("ipv6"));
  EXPECT_THAT(ToString(IpPreference::kNone), IsEmpty());
  EXPECT_THAT(ToString(static_cast<IpPreference>(99)), IsEmpty());
}

TEST(DirectPathProberTest, ProbeNullAuthFails) {
  StatusOr<DirectPathProbeResult> const result = DirectPathProber::Probe(
      nullptr, bigtable::InstanceResource(Project("test-proj"), "test-inst"),
      Options{}, CompletionQueue{});
  EXPECT_THAT(result, StatusIs(StatusCode::kInternal));
}

TEST(DirectPathProberTest, ProbeHonorsTimeoutOption) {
  auto mock_auth = std::make_shared<MockAuthenticationStrategy>();
  EXPECT_CALL(*mock_auth, RequiresConfigureContext())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_auth, CreateChannel(testing::_, testing::_))
      .WillRepeatedly([](std::string const&, grpc::ChannelArguments const&) {
        return grpc::CreateCustomChannel("localhost:1",
                                         grpc::InsecureChannelCredentials(),
                                         grpc::ChannelArguments());
      });

  Options const options =
      Options{}.set<bigtable::experimental::DirectPathProbeTimeoutOption>(
          std::chrono::milliseconds(100));

  StatusOr<DirectPathProbeResult> const result = DirectPathProber::Probe(
      mock_auth, bigtable::InstanceResource(Project("test-proj"), "test-inst"),
      options, CompletionQueue{});
  EXPECT_THAT(result, Not(IsOk()));
}

TEST(DirectPathProberTest, ProbeDefaultTimeoutWhenZeroOrNegative) {
  auto mock_auth = std::make_shared<MockAuthenticationStrategy>();
  EXPECT_CALL(*mock_auth, RequiresConfigureContext())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_auth, CreateChannel(testing::_, testing::_))
      .WillRepeatedly([](std::string const&, grpc::ChannelArguments const&) {
        return grpc::CreateCustomChannel("localhost:1",
                                         grpc::InsecureChannelCredentials(),
                                         grpc::ChannelArguments());
      });

  Options const options =
      Options{}.set<bigtable::experimental::DirectPathProbeTimeoutOption>(
          std::chrono::milliseconds(-10));

  StatusOr<DirectPathProbeResult> const result = DirectPathProber::Probe(
      mock_auth, bigtable::InstanceResource(Project("test-proj"), "test-inst"),
      options, CompletionQueue{});
  EXPECT_THAT(result, Not(IsOk()));
}

TEST(DirectPathProberTest, ProbeSucceedsRpcOverInsecureChannel) {
  FakeBigtableService service;
  grpc::ServerBuilder builder;
  int port = 0;
  builder.AddListeningPort("localhost:0", grpc::InsecureServerCredentials(),
                           &port);
  builder.RegisterService(&service);
  std::unique_ptr<grpc::Server> server = builder.BuildAndStart();
  ASSERT_THAT(server, testing::NotNull());

  auto mock_auth = std::make_shared<MockAuthenticationStrategy>();
  EXPECT_CALL(*mock_auth, RequiresConfigureContext())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_auth, CreateChannel(testing::_, testing::_))
      .WillRepeatedly([port](std::string const&,
                             grpc::ChannelArguments const& args) {
        return grpc::CreateCustomChannel("localhost:" + std::to_string(port),
                                         grpc::InsecureChannelCredentials(),
                                         args);
      });

  StatusOr<DirectPathProbeResult> const result = DirectPathProber::Probe(
      mock_auth, bigtable::InstanceResource(Project("test-proj"), "test-inst"),
      Options{}, CompletionQueue{});

  server->Shutdown();

  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, AllOf(ProbeSuccess(Eq(false)),
                             ProbeIpPreference(Eq(IpPreference::kNone)),
                             ProbePeerAddress(Not(IsEmpty()))));
}

TEST(DirectPathProberTest, ProbePropagatesInstanceAndAppProfileOptions) {
  FakeBigtableService service;
  grpc::ServerBuilder builder;
  int port = 0;
  builder.AddListeningPort("localhost:0", grpc::InsecureServerCredentials(),
                           &port);
  builder.RegisterService(&service);
  std::unique_ptr<grpc::Server> server = builder.BuildAndStart();
  ASSERT_THAT(server, testing::NotNull());

  auto mock_auth = std::make_shared<MockAuthenticationStrategy>();
  EXPECT_CALL(*mock_auth, RequiresConfigureContext())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_auth, CreateChannel(testing::_, testing::_))
      .WillRepeatedly([port](std::string const&,
                             grpc::ChannelArguments const& args) {
        return grpc::CreateCustomChannel("localhost:" + std::to_string(port),
                                         grpc::InsecureChannelCredentials(),
                                         args);
      });

  Options const options =
      Options{}.set<bigtable::AppProfileIdOption>("test-app-profile");
  bigtable::InstanceResource const instance(Project("test-proj"), "test-inst");

  StatusOr<DirectPathProbeResult> const result =
      DirectPathProber::Probe(mock_auth, instance, options, CompletionQueue{});

  server->Shutdown();

  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(service.last_request.name(),
              Eq("projects/test-proj/instances/test-inst"));
  EXPECT_THAT(service.last_request.app_profile_id(), Eq("test-app-profile"));
}

TEST(DirectPathProberTest, ProbeReturnsErrorWhenRpcFails) {
  FakeBigtableService service;
  service.ping_status =
      grpc::Status(grpc::StatusCode::UNAVAILABLE, "service unavailable");
  grpc::ServerBuilder builder;
  int port = 0;
  builder.AddListeningPort("localhost:0", grpc::InsecureServerCredentials(),
                           &port);
  builder.RegisterService(&service);
  std::unique_ptr<grpc::Server> server = builder.BuildAndStart();
  ASSERT_THAT(server, testing::NotNull());

  auto mock_auth = std::make_shared<MockAuthenticationStrategy>();
  EXPECT_CALL(*mock_auth, RequiresConfigureContext())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_auth, CreateChannel(testing::_, testing::_))
      .WillRepeatedly([port](std::string const&,
                             grpc::ChannelArguments const& args) {
        return grpc::CreateCustomChannel("localhost:" + std::to_string(port),
                                         grpc::InsecureChannelCredentials(),
                                         args);
      });

  StatusOr<DirectPathProbeResult> const result = DirectPathProber::Probe(
      mock_auth, bigtable::InstanceResource(Project("test-proj"), "test-inst"),
      Options{}, CompletionQueue{});

  server->Shutdown();

  EXPECT_THAT(result, StatusIs(StatusCode::kUnavailable,
                               HasSubstr("service unavailable")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
