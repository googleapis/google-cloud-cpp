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

#include "google/cloud/storage/internal/grpc/channel_telemetry.h"
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/scoped_log.h"
#include <gmock/gmock.h>
#include <grpcpp/generic/async_generic_service.h>
#include <grpcpp/grpcpp.h>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Each;
using ::testing::HasSubstr;
using ::testing::Not;

auto constexpr kCloudPathEndpoint = "storage.googleapis.com";
auto constexpr kDirectPathEndpoint = "google-c2p:///storage.googleapis.com";
auto constexpr kInterconnectEndpoint =
    "google-c2p:///storage-direct.googleapis.com?force-xds";

struct DetectTransportTypeTestCase {
  std::string endpoint;
  TransportType expected;
};

/// Exercises `DetectTransportType()` over the endpoints the library can
/// produce, including the ones that only look like DirectPath.
class DetectTransportTypeTest
    : public ::testing::TestWithParam<DetectTransportTypeTestCase> {};

/// @test Verify each endpoint maps to the expected transport.
TEST_P(DetectTransportTypeTest, Classify) {
  auto const& test_case = GetParam();
  EXPECT_EQ(DetectTransportType(test_case.endpoint), test_case.expected)
      << "endpoint=" << test_case.endpoint;
}

INSTANTIATE_TEST_SUITE_P(
    ChannelTelemetry, DetectTransportTypeTest,
    ::testing::Values(
        DetectTransportTypeTestCase{"", TransportType::kCloudPath},
        DetectTransportTypeTestCase{kCloudPathEndpoint,
                                    TransportType::kCloudPath},
        DetectTransportTypeTestCase{"https://storage.googleapis.com",
                                    TransportType::kCloudPath},
        DetectTransportTypeTestCase{"private.googleapis.com:443",
                                    TransportType::kCloudPath},
        // Only the `google-c2p` schemes may negotiate DirectPath. A host that
        // merely mentions the feature does not.
        DetectTransportTypeTestCase{"storage.googleapis.com?force-xds",
                                    TransportType::kCloudPath},
        DetectTransportTypeTestCase{kDirectPathEndpoint,
                                    TransportType::kDirectPath},
        DetectTransportTypeTestCase{
            "google-c2p-experimental:///storage.googleapis.com",
            TransportType::kDirectPath},
        DetectTransportTypeTestCase{kInterconnectEndpoint,
                                    TransportType::kDirectPathInterconnect},
        DetectTransportTypeTestCase{
            "google-c2p-experimental:///storage-direct.googleapis.com?"
            "force-xds",
            TransportType::kDirectPathInterconnect},
        // `force-xds` must be a query parameter key, not a substring of
        // another parameter name or value.
        DetectTransportTypeTestCase{
            "google-c2p:///storage-direct.googleapis.com?disable-force-xds",
            TransportType::kDirectPath},
        DetectTransportTypeTestCase{
            "google-c2p:///storage-direct.googleapis.com?foo=force-xds",
            TransportType::kDirectPath},
        DetectTransportTypeTestCase{
            "google-c2p:///storage-direct.googleapis.com?a=1&force-xds=true",
            TransportType::kDirectPathInterconnect},
        // Everything after `#` is a fragment, so this endpoint has no query.
        DetectTransportTypeTestCase{
            "google-c2p:///storage-direct.googleapis.com#frag?force-xds",
            TransportType::kDirectPath}));

TEST(ChannelTelemetry, ToStringIsStable) {
  EXPECT_EQ(ToString(TransportType::kCloudPath), "CloudPath");
  EXPECT_EQ(ToString(TransportType::kDirectPath), "DirectPath");
  EXPECT_EQ(ToString(TransportType::kDirectPathInterconnect),
            "DirectPathInterconnect");
}

/// @test Verify the default endpoint is reported as CloudPath.
TEST(ChannelTelemetry, LogChannelConfigurationCloudPath) {
  testing_util::ScopedLog log;
  LogChannelConfiguration(Options{}
                              .set<EndpointOption>(kCloudPathEndpoint)
                              .set<AuthorityOption>(kCloudPathEndpoint));
  auto const lines = log.ExtractLines();
  EXPECT_THAT(lines,
              Contains(AllOf(HasSubstr("Configured gRPC transport"),
                             HasSubstr("transport_type=CloudPath"),
                             HasSubstr("endpoint=storage.googleapis.com"),
                             HasSubstr("authority=storage.googleapis"))));
  EXPECT_THAT(lines, Each(Not(HasSubstr("DirectPath over Interconnect"))));
}

/// @test Verify a plain `google-c2p` endpoint is reported as DirectPath.
TEST(ChannelTelemetry, LogChannelConfigurationDirectPath) {
  testing_util::ScopedLog log;
  LogChannelConfiguration(Options{}.set<EndpointOption>(kDirectPathEndpoint));
  EXPECT_THAT(log.ExtractLines(),
              Contains(AllOf(HasSubstr("Configured gRPC transport"),
                             HasSubstr("transport_type=DirectPath,"),
                             HasSubstr("authority=(default)"))));
}

/// @test Verify an active Interconnect endpoint is reported without a warning.
TEST(ChannelTelemetry, LogChannelConfigurationInterconnect) {
  testing_util::ScopedLog log;
  LogChannelConfiguration(
      Options{}
          .set<EndpointOption>(kInterconnectEndpoint)
          .set<storage_experimental::DirectPathXdsOverInterconnectOption>(
              true));
  auto const lines = log.ExtractLines();
  EXPECT_THAT(lines,
              Contains(AllOf(HasSubstr("Configured gRPC transport"),
                             HasSubstr("transport_type=DirectPathInterconnect"),
                             HasSubstr(kInterconnectEndpoint))));
  EXPECT_THAT(lines, Each(Not(HasSubstr("DirectPath over Interconnect is"))));
}

/// @test Verify a requested but inactive feature warns instead of claiming
/// success.
TEST(ChannelTelemetry, LogChannelConfigurationRequestedButNotApplied) {
  testing_util::ScopedLog log;
  // This is what happens when the application also sets `EndpointOption`: the
  // application endpoint wins and the feature is silently disabled.
  LogChannelConfiguration(
      Options{}
          .set<EndpointOption>(kCloudPathEndpoint)
          .set<storage_experimental::DirectPathXdsOverInterconnectOption>(
              true));
  auto const lines = log.ExtractLines();
  EXPECT_THAT(lines, Contains(AllOf(
                         HasSubstr("DirectPath over Interconnect is enabled"),
                         HasSubstr("transport_type=CloudPath"),
                         HasSubstr("endpoint=storage.googleapis.com"))));
  EXPECT_THAT(lines, Each(Not(HasSubstr("Configured gRPC transport"))));
}

/// @test Verify a ready channel logs its transport and elapsed time.
TEST(ChannelTelemetry, LogChannelReadySuccess) {
  testing_util::ScopedLog log;
  LogChannelReady(TransportType::kDirectPathInterconnect,
                  std::chrono::milliseconds(1234), Status{});
  EXPECT_THAT(log.ExtractLines(),
              Contains(AllOf(HasSubstr("gRPC channel [0] is ready"),
                             HasSubstr("transport_type=DirectPathInterconnect"),
                             HasSubstr("elapsed_ms=1234"))));
}

/// @test Verify a readiness failure logs the elapsed time and the status.
TEST(ChannelTelemetry, LogChannelReadyFailure) {
  testing_util::ScopedLog log;
  LogChannelReady(
      TransportType::kDirectPath, std::chrono::milliseconds(42),
      internal::DeadlineExceededError("connection timeout", GCP_ERROR_INFO()));
  EXPECT_THAT(log.ExtractLines(),
              Contains(AllOf(HasSubstr("did not become ready"),
                             HasSubstr("transport_type=DirectPath,"),
                             HasSubstr("elapsed_ms=42"),
                             HasSubstr("connection timeout"))));
}

/// @test Verify an empty channel list reports nothing and still completes.
TEST(ChannelTelemetry, StartChannelTelemetryWithoutChannels) {
  testing_util::ScopedLog log;
  CompletionQueue cq;
  // With no channels there is nothing to observe, and nothing to report.
  StartChannelTelemetry(cq, {}, TransportType::kCloudPath,
                        std::chrono::steady_clock::now(),
                        kDefaultChannelReadyTimeout)
      .get();
  EXPECT_THAT(log.ExtractLines(), Each(Not(HasSubstr("channel [0]"))));
}

/// @test Verify a channel that becomes ready logs its transport and latency.
TEST(ChannelTelemetry, StartChannelTelemetryReportsSuccess) {
  grpc::ServerBuilder builder;
  grpc::AsyncGenericService generic_service;
  builder.RegisterAsyncGenericService(&generic_service);
  int selected_port = 0;
  builder.AddListeningPort("localhost:0", grpc::InsecureServerCredentials(),
                           &selected_port);
  std::unique_ptr<grpc::ServerCompletionQueue> srv_cq =
      builder.AddCompletionQueue();
  std::thread srv_thread([&srv_cq] {
    bool ok = false;
    void* placeholder = nullptr;
    while (srv_cq->Next(&placeholder, &ok)) {
    }
  });
  std::unique_ptr<grpc::Server> server = builder.BuildAndStart();

  std::string const endpoint = "localhost:" + std::to_string(selected_port);
  grpc::ChannelArguments arguments;
  arguments.SetInt(GRPC_ARG_USE_LOCAL_SUBCHANNEL_POOL, 1);
  std::shared_ptr<grpc::Channel> const warmup_channel =
      grpc::CreateCustomChannel(endpoint, grpc::InsecureChannelCredentials(),
                                arguments);
  std::chrono::system_clock::time_point const warmup_deadline =
      std::chrono::system_clock::now() + std::chrono::seconds(30);
  if (!warmup_channel->WaitForConnected(warmup_deadline)) {
    server->Shutdown();
    srv_cq->Shutdown();
    srv_thread.join();
    GTEST_SKIP();
  }

  testing_util::ScopedLog log;
  std::vector<std::shared_ptr<grpc::Channel>> const channels{
      grpc::CreateChannel(endpoint, grpc::InsecureChannelCredentials())};
  // The pool shuts down and joins its completion queue threads in its
  // destructor, so they are cleaned up even if an assertion below throws.
  internal::AutomaticallyCreatedBackgroundThreads pool;
  StartChannelTelemetry(
      pool.cq(), channels, TransportType::kDirectPathInterconnect,
      std::chrono::steady_clock::now(), kDefaultChannelReadyTimeout)
      .get();

  server->Shutdown();
  srv_cq->Shutdown();
  srv_thread.join();

  EXPECT_THAT(log.ExtractLines(),
              Contains(AllOf(HasSubstr("gRPC channel [0] is ready"),
                             HasSubstr("transport_type=DirectPathInterconnect"),
                             HasSubstr("elapsed_ms="))));
}

/// @test Verify a channel that never connects still logs its outcome.
TEST(ChannelTelemetry, StartChannelTelemetryReportsFailures) {
  testing_util::ScopedLog log;
  // There is no server at this address, so the channel never becomes ready and
  // the wait ends with `kDeadlineExceeded`. Use a short budget to keep the test
  // fast; the production default is `kDefaultChannelReadyTimeout`.
  std::vector<std::shared_ptr<grpc::Channel>> const channels{
      grpc::CreateChannel("localhost:1", grpc::InsecureChannelCredentials())};
  internal::AutomaticallyCreatedBackgroundThreads pool;
  StartChannelTelemetry(
      pool.cq(), channels, TransportType::kDirectPathInterconnect,
      std::chrono::steady_clock::now(), std::chrono::milliseconds(100))
      .get();

  EXPECT_THAT(
      log.ExtractLines(),
      Contains(AllOf(HasSubstr("did not become ready"),
                     HasSubstr("transport_type=DirectPathInterconnect"))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
