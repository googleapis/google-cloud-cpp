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
#include "google/cloud/storage/internal/grpc/default_options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/scoped_log.h"
#include <gmock/gmock.h>
#include <grpcpp/generic/async_generic_service.h>
#include <grpcpp/grpcpp.h>
#include <chrono>
#include <future>
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
// Spelled out rather than shared with the implementation, so that a change to
// the expected policy name fails this test.
constexpr char const* kXdsLbPolicyName = "xds_cluster_manager_experimental";

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

struct SelectedTransportTypeTestCase {
  TransportType requested_transport;
  std::string lb_policy_name;
  bool grpc_supports_force_xds;
  TransportType expected_transport;
};

class SelectedTransportTypeTest
    : public ::testing::TestWithParam<SelectedTransportTypeTestCase> {};

/// @test Verify each combination resolves to the transport gRPC selected.
TEST_P(SelectedTransportTypeTest, ResolvesSelectedTransport) {
  SelectedTransportTypeTestCase const& test_case = GetParam();
  EXPECT_EQ(SelectedTransportType(test_case.requested_transport,
                                  test_case.lb_policy_name,
                                  test_case.grpc_supports_force_xds),
            test_case.expected_transport)
      << "requested_transport=" << ToString(test_case.requested_transport)
      << ", lb_policy_name=" << test_case.lb_policy_name
      << ", grpc_supports_force_xds=" << test_case.grpc_supports_force_xds;
}

INSTANTIATE_TEST_SUITE_P(
    ChannelTelemetry, SelectedTransportTypeTest,
    ::testing::Values(
        SelectedTransportTypeTestCase{TransportType::kCloudPath, "pick_first",
                                      true, TransportType::kCloudPath},
        // Non-`google-c2p` endpoints never use DirectPath, even if an xDS LB
        // policy is reported.
        SelectedTransportTypeTestCase{TransportType::kCloudPath,
                                      kXdsLbPolicyName, true,
                                      TransportType::kCloudPath},
        // `google-c2p` falls back to DNS (`pick_first`) on hosts that gRPC does
        // not detect as GCE VMs.
        SelectedTransportTypeTestCase{TransportType::kDirectPath, "pick_first",
                                      true, TransportType::kCloudPath},
        SelectedTransportTypeTestCase{TransportType::kDirectPath,
                                      kXdsLbPolicyName, true,
                                      TransportType::kDirectPath},
        SelectedTransportTypeTestCase{TransportType::kDirectPathInterconnect,
                                      kXdsLbPolicyName, true,
                                      TransportType::kDirectPathInterconnect},
        // gRPC < 1.85.0 ignores `force-xds`: on a GCE VM `google-c2p` still
        // selects xDS, yielding standard DirectPath rather than Interconnect.
        SelectedTransportTypeTestCase{TransportType::kDirectPathInterconnect,
                                      kXdsLbPolicyName, false,
                                      TransportType::kDirectPath},
        // On a non-GCE host, gRPC < 1.85.0 ignores `force-xds` and falls back
        // to DNS.
        SelectedTransportTypeTestCase{TransportType::kDirectPathInterconnect,
                                      "pick_first", false,
                                      TransportType::kCloudPath},
        SelectedTransportTypeTestCase{TransportType::kDirectPathInterconnect,
                                      "pick_first", true,
                                      TransportType::kCloudPath},
        // An empty or unrecognized policy name (for example, after gRPC renames
        // the xDS policy, or when an application configures a custom DNS
        // policy) keeps the requested transport, still adjusted for ignored
        // `force-xds`, rather than relabeling the channel.
        SelectedTransportTypeTestCase{TransportType::kDirectPath, "", true,
                                      TransportType::kDirectPath},
        SelectedTransportTypeTestCase{TransportType::kDirectPath,
                                      "xds_cluster_manager", true,
                                      TransportType::kDirectPath},
        SelectedTransportTypeTestCase{TransportType::kDirectPath, "round_robin",
                                      true, TransportType::kDirectPath},
        SelectedTransportTypeTestCase{TransportType::kDirectPathInterconnect,
                                      "", true,
                                      TransportType::kDirectPathInterconnect},
        SelectedTransportTypeTestCase{TransportType::kDirectPathInterconnect,
                                      "", false, TransportType::kDirectPath},
        SelectedTransportTypeTestCase{TransportType::kDirectPathInterconnect,
                                      "round_robin", false,
                                      TransportType::kDirectPath}));

TEST(ChannelTelemetry, ToStringIsStable) {
  EXPECT_EQ(ToString(TransportType::kCloudPath), "CloudPath");
  EXPECT_EQ(ToString(TransportType::kDirectPath), "DirectPath");
  EXPECT_EQ(ToString(TransportType::kDirectPathInterconnect),
            "DirectPathInterconnect");
}

/// @test Verify the default endpoint is logged as a CloudPath request, along
/// with the gRPC version.
TEST(ChannelTelemetry, LogChannelConfigurationCloudPath) {
  testing_util::ScopedLog log;
  LogChannelConfiguration(Options{}
                              .set<EndpointOption>(kCloudPathEndpoint)
                              .set<AuthorityOption>(kCloudPathEndpoint));
  auto const lines = log.ExtractLines();
  EXPECT_THAT(lines,
              Contains(AllOf(HasSubstr("Configured gRPC transport"),
                             HasSubstr("requested_transport_type=CloudPath"),
                             HasSubstr("endpoint=storage.googleapis.com"),
                             HasSubstr("authority=storage.googleapis"),
                             HasSubstr("grpc_version=" + grpc::Version()))));
  // Only the requested transport is known before a channel is ready.
  EXPECT_THAT(lines, Each(Not(HasSubstr(" transport_type="))));
  EXPECT_THAT(lines, Each(Not(HasSubstr("DirectPath over Interconnect"))));
}

/// @test Verify a plain `google-c2p` endpoint is reported as DirectPath.
TEST(ChannelTelemetry, LogChannelConfigurationDirectPath) {
  testing_util::ScopedLog log;
  LogChannelConfiguration(Options{}.set<EndpointOption>(kDirectPathEndpoint));
  EXPECT_THAT(log.ExtractLines(),
              Contains(AllOf(HasSubstr("Configured gRPC transport"),
                             HasSubstr("requested_transport_type=DirectPath,"),
                             HasSubstr("authority=(default)"))));
}

/// @test Verify an active Interconnect endpoint is reported without a warning.
TEST(ChannelTelemetry, LogChannelConfigurationInterconnect) {
  testing_util::ScopedLog log;
  LogChannelConfiguration(
      Options{}
          .set<EndpointOption>(kInterconnectEndpoint)
          .set<storage_experimental::DirectPathXdsOverInterconnectOption>(true),
      /*grpc_supports_force_xds=*/true);
  auto const lines = log.ExtractLines();
  EXPECT_THAT(lines, Contains(AllOf(HasSubstr("Configured gRPC transport"),
                                    HasSubstr("requested_transport_type="
                                              "DirectPathInterconnect"),
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
          .set<storage_experimental::DirectPathXdsOverInterconnectOption>(true),
      /*grpc_supports_force_xds=*/true);
  auto const lines = log.ExtractLines();
  EXPECT_THAT(lines, Contains(AllOf(
                         HasSubstr("DirectPath over Interconnect is enabled"),
                         HasSubstr("requested_transport_type=CloudPath"),
                         HasSubstr("endpoint=storage.googleapis.com"))));
  EXPECT_THAT(lines, Each(Not(HasSubstr("Configured gRPC transport"))));
}

/// @test Verify that when gRPC does not support `force-xds`, enabling the
/// option logs the gRPC version requirement rather than the `EndpointOption`
/// override warning.
TEST(ChannelTelemetry, LogChannelConfigurationUnsupportedGrpc) {
  testing_util::ScopedLog log;
  LogChannelConfiguration(
      Options{}
          .set<EndpointOption>(kCloudPathEndpoint)
          .set<storage_experimental::DirectPathXdsOverInterconnectOption>(true),
      /*grpc_supports_force_xds=*/false);
  std::vector<std::string> const lines = log.ExtractLines();
  EXPECT_THAT(
      lines,
      Contains(AllOf(HasSubstr("DirectPath over Interconnect requires gRPC"),
                     HasSubstr("1.85.0"), HasSubstr(grpc::Version()),
                     HasSubstr("requested_transport_type=CloudPath"),
                     HasSubstr("endpoint=storage.googleapis.com"))));
  EXPECT_THAT(lines, Each(Not(HasSubstr("Configured gRPC transport"))));
  EXPECT_THAT(lines, Each(Not(HasSubstr("`EndpointOption`"))));
}

/// @test Verify that an application-supplied `?force-xds` endpoint also logs a
/// warning when gRPC does not support `force-xds`.
TEST(ChannelTelemetry, LogChannelConfigurationUnsupportedGrpcExplicitEndpoint) {
  testing_util::ScopedLog log;
  LogChannelConfiguration(Options{}.set<EndpointOption>(kInterconnectEndpoint),
                          /*grpc_supports_force_xds=*/false);
  std::vector<std::string> const lines = log.ExtractLines();
  EXPECT_THAT(lines,
              Contains(AllOf(HasSubstr("1.85.0"), HasSubstr(grpc::Version()),
                             HasSubstr("requested_transport_type="
                                       "DirectPathInterconnect"),
                             HasSubstr(kInterconnectEndpoint))));
  EXPECT_THAT(lines, Each(Not(HasSubstr("Configured gRPC transport"))));
}

/// @test Verify no warning is logged on older gRPC when DirectPath over
/// Interconnect is neither requested nor present in the endpoint.
TEST(ChannelTelemetry, LogChannelConfigurationUnsupportedGrpcNotRequested) {
  testing_util::ScopedLog log;
  LogChannelConfiguration(Options{}.set<EndpointOption>(kDirectPathEndpoint),
                          /*grpc_supports_force_xds=*/false);
  std::vector<std::string> const lines = log.ExtractLines();
  EXPECT_THAT(lines, Contains(AllOf(
                         HasSubstr("Configured gRPC transport"),
                         HasSubstr("requested_transport_type=DirectPath,"))));
  EXPECT_THAT(lines, Each(Not(HasSubstr("1.85.0"))));
}

/// @test Verify the 1-argument `LogChannelConfiguration` overload delegates
/// using `GrpcForceXdsIsSupported()`.
TEST(ChannelTelemetry, LogChannelConfigurationFollowsGrpcVersion) {
  testing_util::ScopedLog log;
  LogChannelConfiguration(
      Options{}
          .set<EndpointOption>(kCloudPathEndpoint)
          .set<storage_experimental::DirectPathXdsOverInterconnectOption>(
              true));
  EXPECT_THAT(
      log.ExtractLines(),
      Contains(HasSubstr(GrpcForceXdsIsSupported()
                             ? "DirectPath over Interconnect is enabled"
                             : "DirectPath over Interconnect requires gRPC")));
}

/// @test Verify a ready channel with no transport mismatch logs no explanatory
/// note.
TEST(ChannelTelemetry, LogChannelReadyMatchingTransport) {
  testing_util::ScopedLog log;
  LogChannelReady(TransportType::kDirectPath, TransportType::kDirectPath,
                  kXdsLbPolicyName, std::chrono::milliseconds(1234), Status{});
  EXPECT_THAT(
      log.ExtractLines(),
      Contains(AllOf(
          HasSubstr("gRPC channel [0] is ready. transport_type=DirectPath,"),
          HasSubstr("requested_transport_type=DirectPath,"),
          HasSubstr("lb_policy=xds_cluster_manager_experimental"),
          HasSubstr("elapsed_ms=1234"))));
}

/// @test Verify a ready channel explains why gRPC selected CloudPath when
/// DirectPath was requested.
TEST(ChannelTelemetry, LogChannelReadyDnsFallback) {
  testing_util::ScopedLog log;
  LogChannelReady(TransportType::kDirectPath, TransportType::kCloudPath,
                  "pick_first", std::chrono::milliseconds(22), Status{});
  EXPECT_THAT(log.ExtractLines(),
              Contains(AllOf(HasSubstr("gRPC channel [0] is ready, but gRPC "
                                       "resolved the endpoint with DNS, so "
                                       "DirectPath is not in use."),
                             HasSubstr(" transport_type=CloudPath,"),
                             HasSubstr("requested_transport_type=DirectPath,"),
                             HasSubstr("lb_policy=pick_first"),
                             HasSubstr("elapsed_ms=22"))));
}

/// @test Verify a ready channel explains why gRPC selected DirectPath when
/// DirectPath over Interconnect was requested.
TEST(ChannelTelemetry, LogChannelReadyForceXdsIgnored) {
  testing_util::ScopedLog log;
  LogChannelReady(TransportType::kDirectPathInterconnect,
                  TransportType::kDirectPath, kXdsLbPolicyName,
                  std::chrono::milliseconds(15531), Status{});
  EXPECT_THAT(
      log.ExtractLines(),
      Contains(AllOf(
          HasSubstr("gRPC channel [0] is ready, but this gRPC version ignores "
                    "`force-xds`, so DirectPath over Interconnect is not in "
                    "use."),
          HasSubstr(" transport_type=DirectPath,"),
          HasSubstr("requested_transport_type=DirectPathInterconnect"),
          HasSubstr("lb_policy=xds_cluster_manager_experimental"))));
}

/// @test Verify a mismatch that no selection rule produces gets a neutral note
/// rather than a wrong reason.
///
/// `SelectedTransportType()` never selects `kDirectPath` for a `kCloudPath`
/// request.
TEST(ChannelTelemetry, LogChannelReadyOtherTransportMismatch) {
  testing_util::ScopedLog log;
  LogChannelReady(TransportType::kCloudPath, TransportType::kDirectPath,
                  kXdsLbPolicyName, std::chrono::milliseconds(5), Status{});
  std::vector<std::string> const lines = log.ExtractLines();
  EXPECT_THAT(
      lines,
      Contains(AllOf(
          HasSubstr("gRPC channel [0] is ready, but gRPC selected a different "
                    "transport."),
          HasSubstr(" transport_type=DirectPath,"),
          HasSubstr("requested_transport_type=CloudPath,"))));
  EXPECT_THAT(lines, Each(Not(HasSubstr("force-xds"))));
  EXPECT_THAT(lines, Each(Not(HasSubstr("DNS"))));
}

/// @test Verify that keeping the requested transport on an unrecognized LB
/// policy is called out, so a renamed gRPC policy is visible in the logs.
TEST(ChannelTelemetry, LogChannelReadyUnrecognizedLbPolicy) {
  testing_util::ScopedLog log;
  LogChannelReady(TransportType::kDirectPath, TransportType::kDirectPath,
                  "xds_cluster_manager", std::chrono::milliseconds(9),
                  Status{});
  EXPECT_THAT(
      log.ExtractLines(),
      Contains(AllOf(
          HasSubstr("gRPC channel [0] is ready, but the LB policy was not "
                    "recognized, so transport_type assumes the requested "
                    "transport."),
          HasSubstr(" transport_type=DirectPath,"),
          HasSubstr("requested_transport_type=DirectPath,"),
          HasSubstr("lb_policy=xds_cluster_manager"))));
}

/// @test Verify a CloudPath request gets no note for an unrecognized LB policy:
/// the label never claims DirectPath, so there is nothing to qualify.
TEST(ChannelTelemetry, LogChannelReadyCloudPathIgnoresLbPolicy) {
  testing_util::ScopedLog log;
  LogChannelReady(TransportType::kCloudPath, TransportType::kCloudPath,
                  "round_robin", std::chrono::milliseconds(3), Status{});
  EXPECT_THAT(
      log.ExtractLines(),
      Contains(AllOf(
          HasSubstr("gRPC channel [0] is ready. transport_type=CloudPath,"),
          HasSubstr("lb_policy=round_robin"))));
}

/// @test Verify a readiness failure logs the requested transport and the
/// status, but no selected transport.
TEST(ChannelTelemetry, LogChannelReadyFailure) {
  testing_util::ScopedLog log;
  LogChannelReady(
      TransportType::kDirectPath, TransportType::kDirectPath, kXdsLbPolicyName,
      std::chrono::milliseconds(42),
      internal::DeadlineExceededError("connection timeout", GCP_ERROR_INFO()));
  std::vector<std::string> const lines = log.ExtractLines();
  EXPECT_THAT(
      lines, Contains(AllOf(
                 HasSubstr("did not become ready"),
                 HasSubstr("requested_transport_type=DirectPath,"),
                 HasSubstr("lb_policy=xds_cluster_manager_experimental"),
                 HasSubstr("elapsed_ms=42"), HasSubstr("connection timeout"))));
  EXPECT_THAT(lines, Each(Not(HasSubstr(" transport_type="))));
}

/// @test Verify a readiness failure with an empty LB policy name (as when the
/// channel is released before resolution) logs `(unknown)`.
TEST(ChannelTelemetry, LogChannelReadyFailureUnknownLbPolicy) {
  testing_util::ScopedLog log;
  LogChannelReady(
      TransportType::kDirectPath, TransportType::kDirectPath, "",
      std::chrono::milliseconds(7),
      internal::CancelledError("channel released", GCP_ERROR_INFO()));
  EXPECT_THAT(log.ExtractLines(),
              Contains(AllOf(HasSubstr("did not become ready"),
                             HasSubstr("lb_policy=(unknown)"))));
}

/// @test Verify an empty channel list reports nothing and still completes.
TEST(ChannelTelemetry, StartChannelTelemetryWithoutChannels) {
  testing_util::ScopedLog log;
  CompletionQueue cq;
  // With no channels there is nothing to observe, and nothing to report.
  StartChannelTelemetry(cq, {}, TransportType::kCloudPath,
                        std::chrono::steady_clock::now(),
                        kDefaultChannelReadyTimeout, /*on_ready=*/{})
      .get();
  EXPECT_THAT(log.ExtractLines(), Each(Not(HasSubstr("channel [0]"))));
}

/// @test Verify a channel that becomes ready reports the transport gRPC
/// selected to both the log and `on_ready`.
///
/// Connects an idle channel to an in-process gRPC server so the `status.ok()`
/// branch of `StartChannelTelemetry()` and its latency callback are exercised
/// end-to-end. The caller requests `kDirectPathInterconnect`, but `localhost`
/// resolves via DNS (`pick_first`), just as a `google-c2p` endpoint does when
/// gRPC falls back to DNS, so the selected transport is `kCloudPath`.
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
  int recorded = 0;
  // A sentinel distinct from both the requested and the expected transport, so
  // a missing or wrong callback fails the assertions below.
  TransportType recorded_transport = TransportType::kDirectPath;
  StartChannelTelemetry(
      pool.cq(), channels, TransportType::kDirectPathInterconnect,
      std::chrono::steady_clock::now(), kDefaultChannelReadyTimeout,
      [&recorded, &recorded_transport](TransportType selected_transport,
                                       std::chrono::steady_clock::duration) {
        ++recorded;
        recorded_transport = selected_transport;
      })
      .get();

  server->Shutdown();
  srv_cq->Shutdown();
  srv_thread.join();

  EXPECT_EQ(recorded, 1);
  EXPECT_EQ(recorded_transport, TransportType::kCloudPath);
  EXPECT_THAT(
      log.ExtractLines(),
      Contains(
          AllOf(HasSubstr("gRPC channel [0] is ready"),
                HasSubstr("resolved the endpoint with DNS"),
                HasSubstr(" transport_type=CloudPath,"),
                HasSubstr("requested_transport_type=DirectPathInterconnect"),
                HasSubstr("lb_policy=pick_first"), HasSubstr("elapsed_ms="))));
}

/// @test Verify a channel that never connects is reported, and not recorded.
///
/// This is the timeout path: the wait ends with `kDeadlineExceeded` rather
/// than success, so the outcome must still reach the log while the latency
/// recorder stays untouched.
TEST(ChannelTelemetry, StartChannelTelemetryReportsFailures) {
  testing_util::ScopedLog log;
  // There is no server at this address, so the channel never becomes ready and
  // the wait ends with `kDeadlineExceeded`. Use a short budget to keep the test
  // fast; the production default is `kDefaultChannelReadyTimeout`.
  std::vector<std::shared_ptr<grpc::Channel>> const channels{
      grpc::CreateChannel("localhost:1", grpc::InsecureChannelCredentials())};
  internal::AutomaticallyCreatedBackgroundThreads pool;
  // A failed connection has no meaningful latency, so the recorder must not
  // run. Recording it would skew the histogram towards the timeout value.
  int recorded = 0;
  StartChannelTelemetry(
      pool.cq(), channels, TransportType::kDirectPathInterconnect,
      std::chrono::steady_clock::now(), std::chrono::milliseconds(100),
      [&recorded](TransportType, std::chrono::steady_clock::duration) {
        ++recorded;
      })
      .get();

  EXPECT_EQ(recorded, 0);
  // The LB policy value depends on whether the resolver produced a result
  // within the short budget, so only check that the field is present.
  EXPECT_THAT(log.ExtractLines(),
              Contains(AllOf(HasSubstr("did not become ready"),
                             HasSubstr("requested_transport_type="
                                       "DirectPathInterconnect"),
                             HasSubstr("lb_policy="))));
}

/// @test Verify releasing the channel ends the wait before the timeout.
///
/// Destroying the client while the first channel is still connecting must not
/// keep the channel alive, or delay the completion queue shutdown, for up to
/// `kDefaultChannelReadyTimeout`.
TEST(ChannelTelemetry, StartChannelTelemetryDoesNotExtendChannelLifetime) {
  testing_util::ScopedLog log;
  // There is no server at this address, so the channel never becomes ready.
  std::vector<std::shared_ptr<grpc::Channel>> channels{
      grpc::CreateChannel("localhost:1", grpc::InsecureChannelCredentials())};
  // An abandoned connection attempt has no meaningful latency. Declared before
  // `pool` so it outlives any continuation that might still reference it.
  int recorded = 0;
  internal::AutomaticallyCreatedBackgroundThreads pool;
  future<void> done = StartChannelTelemetry(
      pool.cq(), channels, TransportType::kDirectPathInterconnect,
      std::chrono::steady_clock::now(), kDefaultChannelReadyTimeout,
      [&recorded](TransportType, std::chrono::steady_clock::duration) {
        ++recorded;
      });
  // Release the last reference to the channel, as destroying the client does.
  channels.clear();
  ASSERT_EQ(done.wait_for(std::chrono::seconds(5)), std::future_status::ready);

  EXPECT_EQ(recorded, 0);
  EXPECT_THAT(
      log.ExtractLines(),
      Contains(AllOf(HasSubstr("did not become ready"), HasSubstr("CANCELLED"),
                     HasSubstr("requested_transport_type="
                               "DirectPathInterconnect"),
                     HasSubstr("lb_policy=(unknown)"))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
