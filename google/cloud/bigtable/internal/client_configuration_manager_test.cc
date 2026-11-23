// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/internal/disable_deprecation_warnings.inc"
#include "google/cloud/bigtable/internal/client_configuration_manager.h"
#include "google/cloud/bigtable/testing/mock_bigtable_stub.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/fake_clock.h"
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace v2 = ::google::bigtable::v2;
using ::google::cloud::bigtable::testing::MockBigtableStub;
using ::google::cloud::testing_util::FakeCompletionQueueImpl;
using ::google::cloud::testing_util::FakeSteadyClock;
using ::google::cloud::testing_util::FakeSystemClock;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::testing::Contains;
using ::testing::DoubleEq;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;

constexpr char kInstanceName[] = "projects/test-project/instances/test-inst";
constexpr char kAppProfileId[] = "my profile/1";

TEST(ClientConfigurationManagerDefaultsTest, DefaultConfigurationValues) {
  v2::ClientConfiguration const cfg = DefaultClientConfiguration();
  ASSERT_TRUE(cfg.has_polling_configuration());
  EXPECT_EQ(cfg.polling_configuration().polling_interval().seconds(), 300);
  EXPECT_GT(cfg.polling_configuration().validity_duration().seconds(), 0);
  EXPECT_EQ(cfg.polling_configuration().max_rpc_retry_count(), 5);

  ASSERT_TRUE(cfg.has_session_configuration());
  v2::SessionClientConfiguration const& session = cfg.session_configuration();
  EXPECT_FLOAT_EQ(session.session_load(), 0.0F);

  ASSERT_TRUE(session.has_channel_configuration());
  EXPECT_EQ(session.channel_configuration().min_server_count(), 2);
  EXPECT_EQ(session.channel_configuration().max_server_count(), 25);
  EXPECT_EQ(session.channel_configuration().per_server_session_count(), 10);
  ASSERT_TRUE(
      session.channel_configuration().has_direct_access_with_fallback());
  EXPECT_EQ(session.channel_configuration()
                .direct_access_with_fallback()
                .check_interval()
                .seconds(),
            60);
  EXPECT_FLOAT_EQ(session.channel_configuration()
                      .direct_access_with_fallback()
                      .error_rate_threshold(),
                  0.8F);

  ASSERT_TRUE(session.has_session_pool_configuration());
  v2::SessionClientConfiguration::SessionPoolConfiguration const& pool =
      session.session_pool_configuration();
  EXPECT_FLOAT_EQ(pool.headroom(), 0.5F);
  EXPECT_EQ(pool.min_session_count(), 5);
  EXPECT_EQ(pool.max_session_count(), 400);
  EXPECT_EQ(pool.new_session_creation_budget(), 50);
  EXPECT_EQ(pool.new_session_creation_penalty().seconds(), 60);
  EXPECT_EQ(pool.consecutive_session_failure_threshold(), 10);
  EXPECT_EQ(pool.new_session_queue_length(), 10);
  EXPECT_TRUE(pool.load_balancing_options().has_least_in_flight());

  EXPECT_EQ(cfg.telemetry_configuration().debug_tag_level(),
            v2::TelemetryConfiguration::WARN);
}

TEST(ClientConfigurationManagerDefaultsTest, ResolveDiversionLoads) {
  v2::SessionClientConfiguration session_cfg;
  session_cfg.set_session_load(0.25F);
  DiversionLoads loads = ResolveDiversionLoads(session_cfg);
  EXPECT_THAT(loads.read_row_session_load, DoubleEq(0.25));
  EXPECT_THAT(loads.mutate_row_session_load, DoubleEq(0.25));

  // Global diversion overrides deprecated session_load and clamps to [0, 1].
  session_cfg.mutable_session_diversion_configuration()
      ->mutable_global_diversion()
      ->set_session_load(1.5F);
  loads = ResolveDiversionLoads(session_cfg);
  EXPECT_THAT(loads.read_row_session_load, DoubleEq(1.0));
  EXPECT_THAT(loads.mutate_row_session_load, DoubleEq(1.0));

  // Per-scope diversion overrides per method; unspecified scopes default to 0.
  google::protobuf::Map<std::string, v2::SessionScopeDiversionConfiguration>*
      scopes = session_cfg.mutable_session_diversion_configuration()
                   ->mutable_per_scope_diversion()
                   ->mutable_scope_diversions();
  (*scopes)["method:Bigtable.ReadRow"].set_session_load(0.75F);
  loads = ResolveDiversionLoads(session_cfg);
  EXPECT_THAT(loads.read_row_session_load, DoubleEq(0.75));
  EXPECT_THAT(loads.mutate_row_session_load, DoubleEq(0.0));

  (*scopes)["method:Bigtable.MutateRow"].set_session_load(0.5F);
  loads = ResolveDiversionLoads(session_cfg);
  EXPECT_THAT(loads.read_row_session_load, DoubleEq(0.75));
  EXPECT_THAT(loads.mutate_row_session_load, DoubleEq(0.5));
}

TEST(ClientConfigurationManagerDefaultsTest, EffectivePollingParameters) {
  v2::ClientConfiguration empty_cfg;
  EXPECT_EQ(EffectivePollingInterval(empty_cfg), std::chrono::seconds(300));
  EXPECT_EQ(EffectiveMaxRpcRetryCount(empty_cfg), 5);

  // Enforces minimum polling interval floor (60s).
  v2::ClientConfiguration fast_cfg;
  fast_cfg.mutable_polling_configuration()
      ->mutable_polling_interval()
      ->set_seconds(5);
  fast_cfg.mutable_polling_configuration()->set_max_rpc_retry_count(0);
  EXPECT_EQ(EffectivePollingInterval(fast_cfg), std::chrono::seconds(60));
  // Explicit 0 retry count in PollingConfiguration is honored.
  EXPECT_EQ(EffectiveMaxRpcRetryCount(fast_cfg), 0);

  // Falls back to deprecated scalar polling_interval when PollingConfiguration
  // is absent.
  v2::ClientConfiguration deprecated_cfg;
  deprecated_cfg.mutable_polling_interval()->set_seconds(120);
  EXPECT_EQ(EffectivePollingInterval(deprecated_cfg),
            std::chrono::seconds(120));

  // Huge validity duration is safely capped without overflowing steady_clock.
  v2::ClientConfiguration huge_validity_cfg;
  huge_validity_cfg.mutable_polling_configuration()
      ->mutable_validity_duration()
      ->set_seconds(315576000000LL);
  std::chrono::milliseconds const capped =
      EffectiveValidityDuration(huge_validity_cfg);
  EXPECT_GT(capped, std::chrono::hours(24 * 365));
  std::chrono::steady_clock::time_point const future_tp =
      std::chrono::steady_clock::now() + capped;
  EXPECT_GT(future_tp, std::chrono::steady_clock::now());
}

TEST(ClientConfigurationManagerTest, EagerInitialPollAndPeriodicRefresh) {
  auto fake_cq = std::make_shared<FakeCompletionQueueImpl>();
  auto steady_clock = std::make_shared<FakeSteadyClock>();
  auto system_clock = std::make_shared<FakeSystemClock>();
  std::chrono::system_clock::time_point const base_sys =
      std::chrono::system_clock::now();
  system_clock->SetTime(base_sys);

  auto mock = std::make_shared<MockBigtableStub>();
  ValidateMetadataFixture metadata_fixture;

  v2::ClientConfiguration server_cfg_1;
  server_cfg_1.mutable_polling_configuration()
      ->mutable_polling_interval()
      ->set_seconds(120);
  server_cfg_1.mutable_polling_configuration()
      ->mutable_validity_duration()
      ->set_seconds(600);
  server_cfg_1.mutable_session_configuration()->set_session_load(0.5F);

  v2::ClientConfiguration server_cfg_2 = server_cfg_1;
  server_cfg_2.mutable_session_configuration()->set_session_load(1.0F);

  EXPECT_CALL(*mock, AsyncGetClientConfiguration)
      .WillOnce([&](CompletionQueue&,
                    std::shared_ptr<grpc::ClientContext> const& context,
                    internal::ImmutableOptions const&,
                    v2::GetClientConfigurationRequest const& req,
                    std::shared_ptr<OperationContext> const&) {
        EXPECT_EQ(req.instance_name(), kInstanceName);
        EXPECT_EQ(req.app_profile_id(), kAppProfileId);
        EXPECT_EQ(context->deadline(),
                  base_sys + ClientConfigurationManager::kPollDeadline);
        std::multimap<std::string, std::string> const headers =
            metadata_fixture.GetMetadata(*context);
        EXPECT_THAT(headers, Contains(Pair("google-cloud-resource-prefix",
                                           kInstanceName)));
        EXPECT_THAT(
            headers,
            Contains(Pair("x-goog-request-params",
                          "instance_name=projects%2Ftest-project%2Finstances"
                          "%2Ftest-inst&app_profile_id=my%20profile%2F1")));
        return make_ready_future(make_status_or(server_cfg_1));
      })
      // Second poll returns identical config: refreshes validity without
      // bumping config_seq or re-firing listeners.
      .WillOnce([&](CompletionQueue&,
                    std::shared_ptr<grpc::ClientContext> const&,
                    internal::ImmutableOptions const&,
                    v2::GetClientConfigurationRequest const&,
                    std::shared_ptr<OperationContext> const&) {
        return make_ready_future(make_status_or(server_cfg_1));
      })
      // Third poll returns updated session_load = 1.0.
      .WillOnce([&](CompletionQueue&,
                    std::shared_ptr<grpc::ClientContext> const&,
                    internal::ImmutableOptions const&,
                    v2::GetClientConfigurationRequest const&,
                    std::shared_ptr<OperationContext> const&) {
        return make_ready_future(make_status_or(server_cfg_2));
      });

  std::shared_ptr<ClientConfigurationManager> mgr =
      ClientConfigurationManager::Create(
          CompletionQueue(fake_cq), mock, kInstanceName, kAppProfileId,
          Options{}, ClientConfigurationManager::Config{}, steady_clock,
          system_clock);

  std::vector<std::int64_t> observed_seqs;
  std::vector<double> observed_read_loads;
  ClientConfigurationManager::UnregisterCallback unreg_general =
      mgr->AddListener([&](v2::ClientConfiguration const&, std::int64_t seq) {
        observed_seqs.push_back(seq);
      });
  ClientConfigurationManager::UnregisterCallback unreg_loads =
      mgr->AddDiversionLoadsListener([&](DiversionLoads const& loads) {
        observed_read_loads.push_back(loads.read_row_session_load);
      });

  // Initial registration fired general listener at seq=0, skipped diversion
  // listener at seq=0.
  EXPECT_THAT(observed_seqs, ElementsAre(0));
  EXPECT_THAT(observed_read_loads, IsEmpty());

  mgr->Start();
  EXPECT_EQ(mgr->config_seq(), 1);
  EXPECT_THAT(mgr->GetConfig(), IsProtoEqual(server_cfg_1));
  EXPECT_THAT(observed_seqs, ElementsAre(0, 1));
  EXPECT_THAT(observed_read_loads, ElementsAre(0.5));
  ASSERT_EQ(fake_cq->size(), 1U);

  // Advance steady clock by 300s and trigger the second poll (unchanged proto).
  steady_clock->AdvanceTime(std::chrono::seconds(300));
  std::chrono::steady_clock::time_point const valid_before =
      mgr->GetSnapshot().valid_until;
  fake_cq->SimulateCompletion(true);
  EXPECT_EQ(mgr->config_seq(), 1);
  EXPECT_GT(mgr->GetSnapshot().valid_until, valid_before);
  EXPECT_THAT(observed_seqs, ElementsAre(0, 1));
  EXPECT_THAT(observed_read_loads, ElementsAre(0.5));
  ASSERT_EQ(fake_cq->size(), 1U);

  // Trigger the third poll (changed session_load -> 1.0).
  fake_cq->SimulateCompletion(true);
  EXPECT_EQ(mgr->config_seq(), 2);
  EXPECT_THAT(mgr->GetConfig(), IsProtoEqual(server_cfg_2));
  EXPECT_THAT(observed_seqs, ElementsAre(0, 1, 2));
  EXPECT_THAT(observed_read_loads, ElementsAre(0.5, 1.0));

  unreg_general();
  unreg_loads();
  mgr->Shutdown();
  fake_cq->SimulateCompletion(false);
}

TEST(ClientConfigurationManagerTest, StopPollingHaltsPeriodicTimers) {
  auto fake_cq = std::make_shared<FakeCompletionQueueImpl>();
  auto mock = std::make_shared<MockBigtableStub>();

  v2::ClientConfiguration stop_cfg;
  stop_cfg.set_stop_polling(true);

  EXPECT_CALL(*mock, AsyncGetClientConfiguration)
      .WillOnce([&](CompletionQueue&,
                    std::shared_ptr<grpc::ClientContext> const&,
                    internal::ImmutableOptions const&,
                    v2::GetClientConfigurationRequest const&,
                    std::shared_ptr<OperationContext> const&) {
        return make_ready_future(make_status_or(stop_cfg));
      });

  std::shared_ptr<ClientConfigurationManager> mgr =
      ClientConfigurationManager::Create(CompletionQueue(fake_cq), mock,
                                         kInstanceName, kAppProfileId,
                                         Options{});
  mgr->Start();
  EXPECT_TRUE(mgr->GetConfig().stop_polling());
  // No periodic timer should be queued when stop_polling is true.
  EXPECT_EQ(fake_cq->size(), 0U);
}

TEST(ClientConfigurationManagerTest, RetriesTransientErrorsUpToMaxRetryCount) {
  auto fake_cq = std::make_shared<FakeCompletionQueueImpl>();
  auto mock = std::make_shared<MockBigtableStub>();

  v2::ClientConfiguration custom_default = DefaultClientConfiguration();
  custom_default.mutable_polling_configuration()->set_max_rpc_retry_count(2);

  v2::ClientConfiguration ok_cfg;
  ok_cfg.mutable_polling_configuration()
      ->mutable_polling_interval()
      ->set_seconds(180);
  ok_cfg.mutable_session_configuration()->set_session_load(0.4F);

  int attempts = 0;
  EXPECT_CALL(*mock, AsyncGetClientConfiguration)
      .Times(3)
      .WillRepeatedly([&](CompletionQueue&,
                          std::shared_ptr<grpc::ClientContext> const&,
                          internal::ImmutableOptions const&,
                          v2::GetClientConfigurationRequest const&,
                          std::shared_ptr<OperationContext> const&) {
        ++attempts;
        if (attempts < 3) {
          return make_ready_future(StatusOr<v2::ClientConfiguration>(
              internal::UnavailableError("transient outage")));
        }
        return make_ready_future(make_status_or(ok_cfg));
      });

  ClientConfigurationManager::Config cfg;
  cfg.default_config = custom_default;
  std::shared_ptr<ClientConfigurationManager> mgr =
      ClientConfigurationManager::Create(CompletionQueue(fake_cq), mock,
                                         kInstanceName, kAppProfileId,
                                         Options{}, std::move(cfg));

  mgr->Start();
  EXPECT_EQ(attempts, 1);
  ASSERT_EQ(fake_cq->size(), 1U);

  // Fire retry #1 -> fails -> schedules retry #2.
  fake_cq->SimulateCompletion(true);
  EXPECT_EQ(attempts, 2);
  ASSERT_EQ(fake_cq->size(), 1U);

  // Fire retry #2 -> succeeds -> updates config and schedules next periodic
  // poll.
  fake_cq->SimulateCompletion(true);
  EXPECT_EQ(attempts, 3);
  EXPECT_EQ(mgr->config_seq(), 1);
  EXPECT_FLOAT_EQ(mgr->GetConfig().session_configuration().session_load(),
                  0.4F);

  mgr->Shutdown();
  fake_cq->SimulateCompletion(false);
}

TEST(ClientConfigurationManagerTest, NonRetryableErrorsSkipRetries) {
  auto fake_cq = std::make_shared<FakeCompletionQueueImpl>();
  auto mock = std::make_shared<MockBigtableStub>();

  int attempts = 0;
  EXPECT_CALL(*mock, AsyncGetClientConfiguration)
      .WillOnce([&](CompletionQueue&,
                    std::shared_ptr<grpc::ClientContext> const&,
                    internal::ImmutableOptions const&,
                    v2::GetClientConfigurationRequest const&,
                    std::shared_ptr<OperationContext> const&) {
        ++attempts;
        return make_ready_future(
            StatusOr<v2::ClientConfiguration>(internal::PermissionDeniedError(
                "missing bigtable.instances.ping")));
      });

  std::shared_ptr<ClientConfigurationManager> mgr =
      ClientConfigurationManager::Create(CompletionQueue(fake_cq), mock,
                                         kInstanceName, kAppProfileId,
                                         Options{});
  mgr->Start();
  EXPECT_EQ(attempts, 1);
  ClientConfigurationManager::Snapshot snap = mgr->GetSnapshot();
  ASSERT_TRUE(snap.last_error.has_value());
  EXPECT_THAT(*snap.last_error, StatusIs(StatusCode::kPermissionDenied));
  // Schedules the next periodic poll directly without immediate retry attempts.
  EXPECT_EQ(fake_cq->size(), 1U);

  mgr->Shutdown();
  fake_cq->SimulateCompletion(false);
}

TEST(ClientConfigurationManagerTest,
     FallsBackToDefaultWhenValidityWindowExpires) {
  auto fake_cq = std::make_shared<FakeCompletionQueueImpl>();
  auto steady_clock = std::make_shared<FakeSteadyClock>();
  auto system_clock = std::make_shared<FakeSystemClock>();
  auto mock = std::make_shared<MockBigtableStub>();

  v2::ClientConfiguration server_cfg;
  server_cfg.mutable_polling_configuration()
      ->mutable_polling_interval()
      ->set_seconds(60);
  server_cfg.mutable_polling_configuration()
      ->mutable_validity_duration()
      ->set_seconds(120);
  server_cfg.mutable_polling_configuration()->set_max_rpc_retry_count(0);
  server_cfg.mutable_session_configuration()->set_session_load(0.9F);

  EXPECT_CALL(*mock, AsyncGetClientConfiguration)
      .WillOnce([&](CompletionQueue&,
                    std::shared_ptr<grpc::ClientContext> const&,
                    internal::ImmutableOptions const&,
                    v2::GetClientConfigurationRequest const&,
                    std::shared_ptr<OperationContext> const&) {
        return make_ready_future(make_status_or(server_cfg));
      })
      .WillRepeatedly([&](CompletionQueue&,
                          std::shared_ptr<grpc::ClientContext> const&,
                          internal::ImmutableOptions const&,
                          v2::GetClientConfigurationRequest const&,
                          std::shared_ptr<OperationContext> const&) {
        return make_ready_future(StatusOr<v2::ClientConfiguration>(
            internal::UnavailableError("control plane down")));
      });

  std::shared_ptr<ClientConfigurationManager> mgr =
      ClientConfigurationManager::Create(
          CompletionQueue(fake_cq), mock, kInstanceName, kAppProfileId,
          Options{}, ClientConfigurationManager::Config{}, steady_clock,
          system_clock);

  std::vector<double> load_updates;
  ClientConfigurationManager::UnregisterCallback unreg =
      mgr->AddDiversionLoadsListener([&](DiversionLoads const& loads) {
        load_updates.push_back(loads.read_row_session_load);
      });

  mgr->Start();
  EXPECT_EQ(mgr->config_seq(), 1);
  EXPECT_THAT(load_updates, ElementsAre(DoubleEq(static_cast<double>(0.9F))));

  // Poll fails BEFORE validity expires (at +60s < 120s): keeps server_cfg.
  steady_clock->AdvanceTime(std::chrono::seconds(60));
  fake_cq->SimulateCompletion(true);
  EXPECT_EQ(mgr->config_seq(), 1);
  EXPECT_FLOAT_EQ(mgr->GetConfig().session_configuration().session_load(),
                  0.9F);
  EXPECT_THAT(load_updates, ElementsAre(DoubleEq(static_cast<double>(0.9F))));

  // Poll fails AT/AFTER validity expires (at +130s >= 120s): falls back to
  // DefaultClientConfiguration() (session_load = 0.0).
  steady_clock->AdvanceTime(std::chrono::seconds(70));
  fake_cq->SimulateCompletion(true);
  EXPECT_EQ(mgr->config_seq(), 2);
  EXPECT_THAT(mgr->GetConfig(), IsProtoEqual(DefaultClientConfiguration()));
  EXPECT_THAT(load_updates,
              ElementsAre(DoubleEq(static_cast<double>(0.9F)), DoubleEq(0.0)));

  // Subsequent failed poll does NOT re-trigger fallback or spam listeners
  // because valid_until was reset to the default far-future window.
  steady_clock->AdvanceTime(std::chrono::seconds(300));
  fake_cq->SimulateCompletion(true);
  EXPECT_EQ(mgr->config_seq(), 2);
  EXPECT_THAT(load_updates,
              ElementsAre(DoubleEq(static_cast<double>(0.9F)), DoubleEq(0.0)));

  unreg();
  mgr->Shutdown();
  fake_cq->SimulateCompletion(false);
}

TEST(ClientConfigurationManagerTest,
     SessionPoolListenerSuppressesUnchangedPoolSliceAndAllowsReentrancy) {
  auto fake_cq = std::make_shared<FakeCompletionQueueImpl>();
  auto mock = std::make_shared<MockBigtableStub>();

  v2::ClientConfiguration cfg_a = DefaultClientConfiguration();
  cfg_a.mutable_polling_configuration()
      ->mutable_polling_interval()
      ->set_seconds(120);

  v2::ClientConfiguration cfg_b = cfg_a;
  cfg_b.mutable_session_configuration()
      ->mutable_session_pool_configuration()
      ->set_max_session_count(250);

  EXPECT_CALL(*mock, AsyncGetClientConfiguration)
      .WillOnce([&](CompletionQueue&,
                    std::shared_ptr<grpc::ClientContext> const&,
                    internal::ImmutableOptions const&,
                    v2::GetClientConfigurationRequest const&,
                    std::shared_ptr<OperationContext> const&) {
        return make_ready_future(make_status_or(cfg_a));
      })
      .WillOnce([&](CompletionQueue&,
                    std::shared_ptr<grpc::ClientContext> const&,
                    internal::ImmutableOptions const&,
                    v2::GetClientConfigurationRequest const&,
                    std::shared_ptr<OperationContext> const&) {
        return make_ready_future(make_status_or(cfg_b));
      });

  std::shared_ptr<ClientConfigurationManager> mgr =
      ClientConfigurationManager::Create(CompletionQueue(fake_cq), mock,
                                         kInstanceName, kAppProfileId,
                                         Options{});

  std::vector<int> max_sessions_seen;
  ClientConfigurationManager::UnregisterCallback self_unreg;
  self_unreg = mgr->AddSessionPoolConfigurationListener(
      [&](v2::SessionClientConfiguration::SessionPoolConfiguration const&
              pool_cfg) {
        // Re-entrantly query manager state inside callback to verify no
        // lock-inversion / self-deadlock occurs.
        (void)mgr->GetConfig();
        (void)mgr->GetSnapshot();
        max_sessions_seen.push_back(pool_cfg.max_session_count());
        if (pool_cfg.max_session_count() == 250 && self_unreg) self_unreg();
      });

  // Immediate bootstrap fire delivers default max_session_count = 400.
  EXPECT_THAT(max_sessions_seen, ElementsAre(400));

  // Initial poll changes only polling_interval (300s -> 120s); pool listener
  // must NOT re-fire.
  mgr->Start();
  EXPECT_EQ(mgr->config_seq(), 1);
  EXPECT_THAT(max_sessions_seen, ElementsAre(400));

  // Next poll changes max_session_count -> 250; listener fires and
  // self-unregisters cleanly without deadlock.
  fake_cq->SimulateCompletion(true);
  EXPECT_EQ(mgr->config_seq(), 2);
  EXPECT_THAT(max_sessions_seen, ElementsAre(400, 250));

  mgr->Shutdown();
  fake_cq->SimulateCompletion(false);
}

TEST(ClientConfigurationManagerTest, EnvironmentOverrideMergesIntoConfig) {
  ScopedEnvironment env(
      "BIGTABLE_CLIENT_CONFIG_OVERRIDE",
      "session_configuration { session_diversion_configuration { "
      "global_diversion { session_load: 0.75 } } } stop_polling: true");
  auto fake_cq = std::make_shared<FakeCompletionQueueImpl>();
  auto mock = std::make_shared<MockBigtableStub>();

  std::shared_ptr<ClientConfigurationManager> mgr =
      ClientConfigurationManager::Create(CompletionQueue(fake_cq), mock,
                                         kInstanceName, kAppProfileId,
                                         Options{});
  EXPECT_FLOAT_EQ(mgr->GetConfig()
                      .session_configuration()
                      .session_diversion_configuration()
                      .global_diversion()
                      .session_load(),
                  0.75F);
  EXPECT_TRUE(mgr->GetConfig().stop_polling());
}

TEST(ClientConfigurationManagerTest,
     SingleThreadedCompletionQueueNoDeadlockAndFastShutdown) {
  internal::AutomaticallyCreatedBackgroundThreads bg(1);
  auto mock = std::make_shared<MockBigtableStub>();

  google::cloud::promise<void> second_poll_done;
  std::atomic<int> call_count{0};

  // Poll #1 returns a 15ms interval so Poll #2 fires on the same
  // single-threaded CompletionQueue; Poll #2 schedules a 300s timer that
  // Shutdown() must cancel immediately without hanging bg.Shutdown().
  EXPECT_CALL(*mock, AsyncGetClientConfiguration)
      .WillOnce([&](CompletionQueue& cq,
                    std::shared_ptr<grpc::ClientContext> const&,
                    internal::ImmutableOptions const&,
                    v2::GetClientConfigurationRequest const&,
                    std::shared_ptr<OperationContext> const&) {
        ++call_count;
        return cq.MakeRelativeTimer(std::chrono::milliseconds(5))
            .then([](future<StatusOr<std::chrono::system_clock::time_point>>)
                      -> StatusOr<v2::ClientConfiguration> {
              v2::ClientConfiguration resp;
              resp.mutable_polling_configuration()
                  ->mutable_polling_interval()
                  ->set_nanos(15000000);
              resp.mutable_session_configuration()->set_session_load(0.25F);
              return resp;
            });
      })
      .WillOnce([&](CompletionQueue& cq,
                    std::shared_ptr<grpc::ClientContext> const&,
                    internal::ImmutableOptions const&,
                    v2::GetClientConfigurationRequest const&,
                    std::shared_ptr<OperationContext> const&) {
        ++call_count;
        return cq.MakeRelativeTimer(std::chrono::milliseconds(5))
            .then([&](future<StatusOr<std::chrono::system_clock::time_point>>)
                      -> StatusOr<v2::ClientConfiguration> {
              v2::ClientConfiguration resp;
              resp.mutable_polling_configuration()
                  ->mutable_polling_interval()
                  ->set_seconds(300);
              resp.mutable_session_configuration()->set_session_load(0.75F);
              second_poll_done.set_value();
              return resp;
            });
      });

  ClientConfigurationManager::Config cfg;
  cfg.min_polling_interval = std::chrono::milliseconds(15);
  std::shared_ptr<ClientConfigurationManager> mgr =
      ClientConfigurationManager::Create(bg.cq(), mock, kInstanceName,
                                         kAppProfileId, Options{},
                                         std::move(cfg));

  mgr->Start();
  ASSERT_EQ(second_poll_done.get_future().wait_for(std::chrono::seconds(5)),
            std::future_status::ready);
  EXPECT_EQ(call_count.load(), 2);

  std::chrono::steady_clock::time_point const shutdown_start =
      std::chrono::steady_clock::now();
  mgr->Shutdown();
  bg.Shutdown();
  std::chrono::nanoseconds const shutdown_elapsed =
      std::chrono::steady_clock::now() - shutdown_start;
  EXPECT_LT(shutdown_elapsed, std::chrono::seconds(2));
}

TEST(ClientConfigurationManagerTest,
     DiversionLoadsListenerFiresImmediatelyOnLateRegistration) {
  auto fake_cq = std::make_shared<FakeCompletionQueueImpl>();
  auto mock = std::make_shared<MockBigtableStub>();

  v2::ClientConfiguration server_cfg_1;
  server_cfg_1.mutable_polling_configuration()
      ->mutable_polling_interval()
      ->set_seconds(120);
  server_cfg_1.mutable_session_configuration()->set_session_load(0.6F);

  v2::ClientConfiguration server_cfg_2 = server_cfg_1;
  server_cfg_2.mutable_session_configuration()->set_session_load(0.8F);
  server_cfg_2.set_stop_polling(true);

  EXPECT_CALL(*mock, AsyncGetClientConfiguration)
      .WillOnce([&](CompletionQueue&,
                    std::shared_ptr<grpc::ClientContext> const&,
                    internal::ImmutableOptions const&,
                    v2::GetClientConfigurationRequest const&,
                    std::shared_ptr<OperationContext> const&) {
        return make_ready_future(make_status_or(server_cfg_1));
      })
      .WillOnce([&](CompletionQueue&,
                    std::shared_ptr<grpc::ClientContext> const&,
                    internal::ImmutableOptions const&,
                    v2::GetClientConfigurationRequest const&,
                    std::shared_ptr<OperationContext> const&) {
        return make_ready_future(make_status_or(server_cfg_1));
      })
      .WillOnce([&](CompletionQueue&,
                    std::shared_ptr<grpc::ClientContext> const&,
                    internal::ImmutableOptions const&,
                    v2::GetClientConfigurationRequest const&,
                    std::shared_ptr<OperationContext> const&) {
        return make_ready_future(make_status_or(server_cfg_2));
      });

  std::shared_ptr<ClientConfigurationManager> mgr =
      ClientConfigurationManager::Create(CompletionQueue(fake_cq), mock,
                                         kInstanceName, kAppProfileId,
                                         Options{});

  // Start completes the initial poll synchronously (seq == 1) before any
  // DiversionLoadsListener is registered.
  mgr->Start();
  ASSERT_EQ(mgr->config_seq(), 1);

  std::vector<double> late_loads;
  ClientConfigurationManager::UnregisterCallback unreg_late =
      mgr->AddDiversionLoadsListener([&](DiversionLoads const& loads) {
        late_loads.push_back(loads.read_row_session_load);
      });
  EXPECT_THAT(late_loads, ElementsAre(DoubleEq(static_cast<double>(0.6F))));

  // Unchanged subsequent poll does not re-fire the late listener.
  fake_cq->SimulateCompletion(true);
  EXPECT_EQ(mgr->config_seq(), 1);
  EXPECT_THAT(late_loads, ElementsAre(DoubleEq(static_cast<double>(0.6F))));

  // Updated poll with stop_polling=true delivers 0.8, and a listener registered
  // after polling has stopped still immediately receives 0.8.
  fake_cq->SimulateCompletion(true);
  EXPECT_EQ(mgr->config_seq(), 2);
  EXPECT_THAT(late_loads, ElementsAre(DoubleEq(static_cast<double>(0.6F)),
                                      DoubleEq(static_cast<double>(0.8F))));

  std::vector<double> post_stop_loads;
  ClientConfigurationManager::UnregisterCallback unreg_post_stop =
      mgr->AddDiversionLoadsListener([&](DiversionLoads const& loads) {
        post_stop_loads.push_back(loads.read_row_session_load);
      });
  EXPECT_THAT(post_stop_loads,
              ElementsAre(DoubleEq(static_cast<double>(0.8F))));

  unreg_late();
  unreg_post_stop();
  mgr->Shutdown();
}

TEST(ClientConfigurationManagerTest,
     InitialPollMatchingDefaultConfigAdvancesSeqAndNotifiesListeners) {
  auto fake_cq = std::make_shared<FakeCompletionQueueImpl>();
  auto mock = std::make_shared<MockBigtableStub>();

  v2::ClientConfiguration const default_cfg = DefaultClientConfiguration();
  EXPECT_CALL(*mock, AsyncGetClientConfiguration)
      .Times(2)
      .WillRepeatedly([&](CompletionQueue&,
                          std::shared_ptr<grpc::ClientContext> const&,
                          internal::ImmutableOptions const&,
                          v2::GetClientConfigurationRequest const&,
                          std::shared_ptr<OperationContext> const&) {
        return make_ready_future(make_status_or(default_cfg));
      });

  std::shared_ptr<ClientConfigurationManager> mgr =
      ClientConfigurationManager::Create(CompletionQueue(fake_cq), mock,
                                         kInstanceName, kAppProfileId,
                                         Options{});

  std::vector<std::int64_t> observed_seqs;
  std::vector<double> observed_loads;
  ClientConfigurationManager::UnregisterCallback unreg_general =
      mgr->AddListener([&](v2::ClientConfiguration const&, std::int64_t seq) {
        observed_seqs.push_back(seq);
      });
  ClientConfigurationManager::UnregisterCallback unreg_loads =
      mgr->AddDiversionLoadsListener([&](DiversionLoads const& loads) {
        observed_loads.push_back(loads.read_row_session_load);
      });

  EXPECT_THAT(observed_seqs, ElementsAre(0));
  EXPECT_THAT(observed_loads, IsEmpty());

  // First poll returns DefaultClientConfiguration(), matching current_config_,
  // yet still advances config_seq from 0 to 1 and notifies listeners.
  mgr->Start();
  EXPECT_EQ(mgr->config_seq(), 1);
  EXPECT_THAT(observed_seqs, ElementsAre(0, 1));
  EXPECT_THAT(observed_loads, ElementsAre(DoubleEq(0.0)));

  // Second poll also returns DefaultClientConfiguration(): now that seq > 0,
  // duplicate config notifications are suppressed.
  fake_cq->SimulateCompletion(true);
  EXPECT_EQ(mgr->config_seq(), 1);
  EXPECT_THAT(observed_seqs, ElementsAre(0, 1));
  EXPECT_THAT(observed_loads, ElementsAre(DoubleEq(0.0)));

  unreg_general();
  unreg_loads();
  mgr->Shutdown();
  fake_cq->SimulateCompletion(false);
}

TEST(ClientConfigurationManagerTest,
     UnregisterCallbackDrainsInFlightCallbackOnAnotherThread) {
  internal::AutomaticallyCreatedBackgroundThreads bg(1);
  auto mock = std::make_shared<MockBigtableStub>();

  google::cloud::promise<void> callback_entered;
  google::cloud::future<void> callback_entered_future =
      callback_entered.get_future();
  google::cloud::promise<void> allow_callback_exit;
  google::cloud::future<void> allow_callback_exit_future =
      allow_callback_exit.get_future();
  google::cloud::promise<void> unregister_started;
  google::cloud::future<void> unregister_started_future =
      unregister_started.get_future();
  google::cloud::promise<void> unregister_returned;
  google::cloud::future<void> unregister_returned_future =
      unregister_returned.get_future();

  std::atomic<bool> callback_finished{false};
  std::atomic<int> post_bootstrap_calls{0};

  EXPECT_CALL(*mock, AsyncGetClientConfiguration)
      .WillOnce([&](CompletionQueue& cq,
                    std::shared_ptr<grpc::ClientContext> const&,
                    internal::ImmutableOptions const&,
                    v2::GetClientConfigurationRequest const&,
                    std::shared_ptr<OperationContext> const&) {
        return cq.MakeRelativeTimer(std::chrono::milliseconds(1))
            .then([](future<StatusOr<std::chrono::system_clock::time_point>>)
                      -> StatusOr<v2::ClientConfiguration> {
              v2::ClientConfiguration resp;
              resp.mutable_session_configuration()->set_session_load(0.5F);
              return resp;
            });
      });

  std::shared_ptr<ClientConfigurationManager> mgr =
      ClientConfigurationManager::Create(bg.cq(), mock, kInstanceName,
                                         kAppProfileId, Options{});

  ClientConfigurationManager::UnregisterCallback unreg = mgr->AddListener(
      [&](v2::ClientConfiguration const&, std::int64_t seq) {
        if (seq == 0) return;
        ++post_bootstrap_calls;
        callback_entered.set_value();
        allow_callback_exit_future.get();
        callback_finished.store(true, std::memory_order_release);
      },
      /*fire_immediately=*/false);

  mgr->Start();
  ASSERT_EQ(callback_entered_future.wait_for(std::chrono::seconds(5)),
            std::future_status::ready);

  std::atomic<bool> finished_before_unreg_returned{false};
  std::thread unreg_thread([&]() {
    unregister_started.set_value();
    unreg();
    finished_before_unreg_returned.store(
        callback_finished.load(std::memory_order_acquire),
        std::memory_order_release);
    unregister_returned.set_value();
  });

  ASSERT_EQ(unregister_started_future.wait_for(std::chrono::seconds(5)),
            std::future_status::ready);
  // While the callback is blocked on the CQ thread, unreg() must not return.
  EXPECT_EQ(unregister_returned_future.wait_for(std::chrono::milliseconds(20)),
            std::future_status::timeout);

  allow_callback_exit.set_value();
  ASSERT_EQ(unregister_returned_future.wait_for(std::chrono::seconds(5)),
            std::future_status::ready);
  unreg_thread.join();

  EXPECT_TRUE(finished_before_unreg_returned.load(std::memory_order_acquire));
  EXPECT_EQ(post_bootstrap_calls.load(), 1);

  mgr->Shutdown();
  bg.Shutdown();
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
