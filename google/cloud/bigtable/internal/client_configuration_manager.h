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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_CLIENT_CONFIGURATION_MANAGER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_CLIENT_CONFIGURATION_MANAGER_H

#include "google/cloud/bigtable/internal/bigtable_stub.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/clock.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/options.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/bigtable/v2/session.pb.h"
#include <grpcpp/client_context.h>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// Returns the built-in default `ClientConfiguration` served prior to the first
/// successful poll and when a previously fetched configuration expires.
google::bigtable::v2::ClientConfiguration DefaultClientConfiguration();

/// Per-method traffic diversion ratios in `[0.0, 1.0]` resolved from a
/// `SessionClientConfiguration`.
struct DiversionLoads {
  double read_row_session_load = 0.0;
  double mutate_row_session_load = 0.0;

  friend bool operator==(DiversionLoads const& lhs, DiversionLoads const& rhs) {
    return lhs.read_row_session_load == rhs.read_row_session_load &&
           lhs.mutate_row_session_load == rhs.mutate_row_session_load;
  }
  friend bool operator!=(DiversionLoads const& lhs, DiversionLoads const& rhs) {
    return !(lhs == rhs);
  }
};

/// Resolves per-method session diversion loads from `config`, honoring
/// `session_diversion_configuration` (`global_diversion` and
/// `per_scope_diversion`) over the deprecated scalar `session_load`.
DiversionLoads ResolveDiversionLoads(
    google::bigtable::v2::SessionClientConfiguration const& config);

/// Returns the effective poll interval from `config`, preferring
/// `polling_configuration.polling_interval` over deprecated `polling_interval`
/// and clamping at `min_interval`.
std::chrono::milliseconds EffectivePollingInterval(
    google::bigtable::v2::ClientConfiguration const& config,
    std::chrono::milliseconds min_interval = std::chrono::minutes(1));

/// Returns the effective validity window from `config`, capped so adding it to
/// `steady_clock::now()` cannot overflow.
std::chrono::milliseconds EffectiveValidityDuration(
    google::bigtable::v2::ClientConfiguration const& config);

/// Returns the per-poll RPC retry budget from `config`, honoring an explicit
/// `0` when `polling_configuration` is present.
int EffectiveMaxRpcRetryCount(
    google::bigtable::v2::ClientConfiguration const& config);

struct ClientConfigurationManagerConfig {
  std::chrono::milliseconds poll_deadline =
      std::chrono::milliseconds(std::chrono::seconds(5));
  std::chrono::milliseconds min_polling_interval =
      std::chrono::milliseconds(std::chrono::minutes(1));
  std::chrono::milliseconds initial_retry_backoff =
      std::chrono::milliseconds(100);
  std::chrono::milliseconds max_retry_backoff =
      std::chrono::milliseconds(std::chrono::seconds(30));
  google::bigtable::v2::ClientConfiguration default_config =
      DefaultClientConfiguration();
  std::optional<google::bigtable::v2::ClientConfiguration> override_config;
};

/**
 * Periodically polls `Bigtable.GetClientConfiguration` asynchronously on a
 * `CompletionQueue` for an `(instance_name, app_profile_id)` pair, maintains
 * configuration validity/fallback state, and fans out changes to listeners.
 */
class ClientConfigurationManager
    : public std::enable_shared_from_this<ClientConfigurationManager> {
 public:
  using SteadyClock = ::google::cloud::internal::SteadyClock;
  using SystemClock = ::google::cloud::internal::SystemClock;
  using Config = ClientConfigurationManagerConfig;

  static constexpr auto kMinPollingInterval =
      std::chrono::milliseconds(std::chrono::minutes(1));
  static constexpr auto kPollDeadline =
      std::chrono::milliseconds(std::chrono::seconds(5));
  static constexpr auto kInitialRetryBackoff = std::chrono::milliseconds(100);
  static constexpr auto kMaxRetryBackoff =
      std::chrono::milliseconds(std::chrono::seconds(30));
  static constexpr std::size_t kMaxPollHistory = 100;

  struct PollEvent {
    std::chrono::system_clock::time_point at;
    std::chrono::milliseconds duration{0};
    Status status;
    std::int64_t config_seq = 0;
  };

  struct Snapshot {
    std::string instance_name;
    std::string app_profile_id;
    std::int64_t config_seq = 0;
    std::chrono::steady_clock::time_point valid_until;
    std::optional<google::bigtable::v2::ClientConfiguration> last_response;
    std::optional<std::chrono::system_clock::time_point> last_fetched_at;
    std::optional<Status> last_error;
    std::optional<std::chrono::system_clock::time_point> last_error_at;
    std::vector<PollEvent> poll_history;
    std::chrono::system_clock::time_point captured_at;
  };

  using ConfigListener = std::function<void(
      google::bigtable::v2::ClientConfiguration const&, std::int64_t seq)>;
  using SessionPoolConfigurationListener =
      std::function<void(google::bigtable::v2::SessionClientConfiguration::
                             SessionPoolConfiguration const&)>;
  using DiversionLoadsListener = std::function<void(DiversionLoads const&)>;
  using UnregisterCallback = std::function<void()>;

  static std::shared_ptr<ClientConfigurationManager> Create(
      CompletionQueue cq, std::shared_ptr<BigtableStub> stub,
      std::string instance_name, std::string app_profile_id,
      Options options = {}, Config config = {},
      std::shared_ptr<SteadyClock> steady_clock =
          std::make_shared<SteadyClock>(),
      std::shared_ptr<SystemClock> system_clock =
          std::make_shared<SystemClock>());

  ~ClientConfigurationManager();

  /// Starts the eager initial poll and schedules periodic background refreshes.
  /// Idempotent.
  void Start();

  /// Cancels any pending poll/backoff timer and in-flight RPC, and drains any
  /// in-flight listener notifications. Idempotent; no listener callback will
  /// execute after `Shutdown()` returns.
  void Shutdown();

  bool IsShutdown() const {
    return shutdown_flag_.load(std::memory_order_acquire);
  }

  std::string const& instance_name() const { return instance_name_; }
  std::string const& app_profile_id() const { return app_profile_id_; }

  google::bigtable::v2::ClientConfiguration GetConfig() const;
  std::int64_t config_seq() const;
  Snapshot GetSnapshot() const;

  /// Registers a general configuration listener. When `fire_immediately` is
  /// true, delivers the current `(config, seq)` synchronously before returning
  /// (unless a newer configuration was concurrently delivered first).
  /// Listeners may safely call `GetConfig()`, `GetSnapshot()`, or their own
  /// unregister callback from inside the callback without deadlocking.
  UnregisterCallback AddListener(ConfigListener listener,
                                 bool fire_immediately = true);

  /// Registers a listener for `SessionPoolConfiguration` changes, firing
  /// immediately with the current/default pool configuration and suppressing
  /// notifications when unrelated `ClientConfiguration` fields change.
  UnregisterCallback AddSessionPoolConfigurationListener(
      SessionPoolConfigurationListener listener);

  /// Registers a listener for server-driven `DiversionLoads` updates, skipping
  /// the bootstrap `seq == 0` configuration and suppressing unchanged loads.
  UnregisterCallback AddDiversionLoadsListener(DiversionLoadsListener listener);

 private:
  struct ListenerSlot {
    explicit ListenerSlot(ConfigListener cb) : callback(std::move(cb)) {}

    std::mutex delivery_mu;
    std::atomic<bool> active{true};
    std::atomic<std::thread::id> delivering_thread{};
    std::int64_t last_delivered_seq = -1;
    ConfigListener callback;
  };

  ClientConfigurationManager(CompletionQueue cq,
                             std::shared_ptr<BigtableStub> stub,
                             std::string instance_name,
                             std::string app_profile_id, Options options,
                             Config config,
                             std::shared_ptr<SteadyClock> steady_clock,
                             std::shared_ptr<SystemClock> system_clock);

  google::bigtable::v2::ClientConfiguration ApplyOverrides(
      google::bigtable::v2::ClientConfiguration cfg) const;

  void StartFetchAttempt(std::unique_lock<std::mutex>& lock, int attempt,
                         std::chrono::steady_clock::time_point poll_start);
  void OnFetchAttemptCompleted(
      std::uint64_t op_id, int attempt,
      std::chrono::steady_clock::time_point poll_start,
      StatusOr<google::bigtable::v2::ClientConfiguration> result);
  void ScheduleRetry(std::unique_lock<std::mutex>& lock, int next_attempt,
                     std::chrono::steady_clock::time_point poll_start);
  void ScheduleNextPoll(std::unique_lock<std::mutex>& lock);
  void ScheduleTimer(std::unique_lock<std::mutex>& lock,
                     std::chrono::milliseconds delay,
                     std::function<void(std::unique_lock<std::mutex>&)> action);
  std::chrono::milliseconds ComputeRetryBackoff(int attempt);
  void RecordPollLocked(PollEvent event);
  void DeliverToSlot(ListenerSlot& slot,
                     google::bigtable::v2::ClientConfiguration const& config,
                     std::int64_t seq) const;

  CompletionQueue cq_;
  std::shared_ptr<BigtableStub> stub_;
  std::string instance_name_;
  std::string app_profile_id_;
  internal::ImmutableOptions options_;
  Config config_;
  std::shared_ptr<SteadyClock> steady_clock_;
  std::shared_ptr<SystemClock> system_clock_;
  std::atomic<bool> shutdown_flag_{false};

  mutable std::mutex mu_;
  bool started_ = false;
  bool shutdown_ = false;
  google::bigtable::v2::ClientConfiguration current_config_;
  std::int64_t config_seq_ = 0;
  std::chrono::steady_clock::time_point valid_until_;
  std::uint64_t timer_generation_ = 0;
  std::uint64_t op_generation_ = 0;
  future<void> timer_;
  std::shared_ptr<grpc::ClientContext> active_rpc_context_;
  internal::DefaultPRNG prng_;

  std::uint64_t next_listener_id_ = 0;
  std::map<std::uint64_t, std::shared_ptr<ListenerSlot>> listeners_;

  std::optional<google::bigtable::v2::ClientConfiguration> last_response_;
  std::optional<std::chrono::system_clock::time_point> last_fetched_at_;
  std::optional<Status> last_error_;
  std::optional<std::chrono::system_clock::time_point> last_error_at_;
  std::deque<PollEvent> poll_history_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_CLIENT_CONFIGURATION_MANAGER_H
