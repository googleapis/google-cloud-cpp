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
#include "google/cloud/bigtable/internal/operation_context.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/url_encode.h"
#include "google/cloud/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <algorithm>
#include <limits>
#include <random>
#include <utility>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

// Safe 100-year cap so adding validity/polling durations to clock::now() never
// overflows int64 nanoseconds.
constexpr std::int64_t kMaxValiditySeconds = 100LL * 365 * 24 * 3600;
constexpr std::chrono::milliseconds kMaxValidityDuration =
    std::chrono::milliseconds(std::chrono::seconds(kMaxValiditySeconds));
constexpr std::chrono::milliseconds kMaxPollingInterval =
    std::chrono::milliseconds(std::chrono::hours(24 * 365));

constexpr char kReadRowScopeKey[] = "method:Bigtable.ReadRow";
constexpr char kMutateRowScopeKey[] = "method:Bigtable.MutateRow";
constexpr char kOverrideEnvVar[] = "BIGTABLE_CLIENT_CONFIG_OVERRIDE";

std::chrono::milliseconds ProtoDurationToMillisCapped(
    google::protobuf::Duration const& d,
    std::chrono::milliseconds max_duration) {
  if (d.seconds() < 0 || (d.seconds() == 0 && d.nanos() < 0)) {
    return std::chrono::milliseconds(0);
  }
  std::int64_t const max_seconds =
      std::chrono::duration_cast<std::chrono::seconds>(max_duration).count();
  if (d.seconds() >= max_seconds) return max_duration;
  std::int64_t const total_ms =
      d.seconds() * 1000LL + static_cast<std::int64_t>(d.nanos()) / 1000000LL;
  return (std::min)(std::chrono::milliseconds(total_ms), max_duration);
}

double ClampLoad(float value) {
  return std::clamp(static_cast<double>(value), 0.0, 1.0);
}

bool IsRetryableConfigStatus(StatusCode code) {
  switch (code) {
    case StatusCode::kOk:
    case StatusCode::kCancelled:
    case StatusCode::kInvalidArgument:
    case StatusCode::kNotFound:
    case StatusCode::kPermissionDenied:
    case StatusCode::kUnauthenticated:
    case StatusCode::kUnimplemented:
      return false;
    default:
      return true;
  }
}

Options SanitizeConfigPollOptions(Options options) {
  options.unset<bigtable::DeadlineOption>();
  return options;
}

std::optional<google::bigtable::v2::ClientConfiguration>
LoadEnvOverrideConfiguration() {
  std::optional<std::string> env = internal::GetEnv(kOverrideEnvVar);
  if (!env.has_value() || env->empty()) return std::nullopt;
  google::bigtable::v2::ClientConfiguration parsed;
  if (!google::protobuf::TextFormat::ParseFromString(*env, &parsed)) {
    GCP_LOG(WARNING) << "Failed to parse " << kOverrideEnvVar
                     << " textproto; ignoring override";
    return std::nullopt;
  }
  return parsed;
}

}  // namespace

google::bigtable::v2::ClientConfiguration DefaultClientConfiguration() {
  google::bigtable::v2::ClientConfiguration cfg;

  google::bigtable::v2::ClientConfiguration::PollingConfiguration* polling =
      cfg.mutable_polling_configuration();
  polling->mutable_polling_interval()->set_seconds(300);
  polling->mutable_validity_duration()->set_seconds(kMaxValiditySeconds);
  polling->set_max_rpc_retry_count(5);

  google::bigtable::v2::SessionClientConfiguration* session =
      cfg.mutable_session_configuration();
  session->set_session_load(0.0F);

  google::bigtable::v2::SessionClientConfiguration::ChannelPoolConfiguration*
      channel = session->mutable_channel_configuration();
  channel->set_min_server_count(2);
  channel->set_max_server_count(25);
  channel->set_per_server_session_count(10);
  google::bigtable::v2::SessionClientConfiguration::ChannelPoolConfiguration::
      DirectAccessWithFallback* fallback =
          channel->mutable_direct_access_with_fallback();
  fallback->mutable_check_interval()->set_seconds(60);
  fallback->set_error_rate_threshold(0.8F);

  google::bigtable::v2::SessionClientConfiguration::SessionPoolConfiguration*
      pool = session->mutable_session_pool_configuration();
  pool->set_headroom(0.5F);
  pool->set_min_session_count(5);
  pool->set_max_session_count(400);
  pool->set_new_session_creation_budget(50);
  pool->mutable_new_session_creation_penalty()->set_seconds(60);
  pool->set_consecutive_session_failure_threshold(10);
  pool->set_new_session_queue_length(10);
  pool->mutable_load_balancing_options()
      ->mutable_least_in_flight()
      ->set_random_subset_size(0);

  cfg.mutable_telemetry_configuration()->set_debug_tag_level(
      google::bigtable::v2::TelemetryConfiguration::WARN);

  return cfg;
}

DiversionLoads ResolveDiversionLoads(
    google::bigtable::v2::SessionClientConfiguration const& config) {
  DiversionLoads loads;
  loads.read_row_session_load = ClampLoad(config.session_load());
  loads.mutate_row_session_load = ClampLoad(config.session_load());

  if (!config.has_session_diversion_configuration()) return loads;

  google::bigtable::v2::SessionDiversionConfiguration const& div =
      config.session_diversion_configuration();
  switch (div.diversion_strategy_case()) {
    case google::bigtable::v2::SessionDiversionConfiguration::
        kGlobalDiversion: {
      double const global_load =
          ClampLoad(div.global_diversion().session_load());
      loads.read_row_session_load = global_load;
      loads.mutate_row_session_load = global_load;
      break;
    }
    case google::bigtable::v2::SessionDiversionConfiguration::
        kPerScopeDiversion: {
      google::protobuf::Map<
          std::string,
          google::bigtable::v2::SessionScopeDiversionConfiguration> const&
          scopes = div.per_scope_diversion().scope_diversions();
      auto const read_it = scopes.find(kReadRowScopeKey);
      loads.read_row_session_load =
          read_it != scopes.end() ? ClampLoad(read_it->second.session_load())
                                  : 0.0;
      auto const mutate_it = scopes.find(kMutateRowScopeKey);
      loads.mutate_row_session_load =
          mutate_it != scopes.end()
              ? ClampLoad(mutate_it->second.session_load())
              : 0.0;
      break;
    }
    case google::bigtable::v2::SessionDiversionConfiguration::
        DIVERSION_STRATEGY_NOT_SET:
      break;
  }
  return loads;
}

std::chrono::milliseconds EffectivePollingInterval(
    google::bigtable::v2::ClientConfiguration const& config) {
  return EffectivePollingInterval(
      config, ClientConfigurationManager::kMinPollingInterval);
}

std::chrono::milliseconds EffectivePollingInterval(
    google::bigtable::v2::ClientConfiguration const& config,
    std::chrono::milliseconds min_interval) {
  std::chrono::milliseconds interval{0};
  if (config.has_polling_configuration() &&
      config.polling_configuration().has_polling_interval()) {
    interval = ProtoDurationToMillisCapped(
        config.polling_configuration().polling_interval(), kMaxPollingInterval);
  } else if (config.has_polling_interval()) {
    interval = ProtoDurationToMillisCapped(config.polling_interval(),
                                           kMaxPollingInterval);
  } else {
    google::bigtable::v2::ClientConfiguration const defaults =
        DefaultClientConfiguration();
    interval = ProtoDurationToMillisCapped(
        defaults.polling_configuration().polling_interval(),
        kMaxPollingInterval);
  }
  return (std::max)(interval, min_interval);
}

std::chrono::milliseconds EffectiveValidityDuration(
    google::bigtable::v2::ClientConfiguration const& config) {
  if (config.has_polling_configuration() &&
      config.polling_configuration().has_validity_duration()) {
    return ProtoDurationToMillisCapped(
        config.polling_configuration().validity_duration(),
        kMaxValidityDuration);
  }
  google::bigtable::v2::ClientConfiguration const defaults =
      DefaultClientConfiguration();
  return ProtoDurationToMillisCapped(
      defaults.polling_configuration().validity_duration(),
      kMaxValidityDuration);
}

int EffectiveMaxRpcRetryCount(
    google::bigtable::v2::ClientConfiguration const& config) {
  if (config.has_polling_configuration()) {
    return (std::max)(0, config.polling_configuration().max_rpc_retry_count());
  }
  return DefaultClientConfiguration()
      .polling_configuration()
      .max_rpc_retry_count();
}

std::shared_ptr<ClientConfigurationManager> ClientConfigurationManager::Create(
    CompletionQueue cq, std::shared_ptr<BigtableStub> stub,
    std::string instance_name, std::string app_profile_id, Options options) {
  return Create(std::move(cq), std::move(stub), std::move(instance_name),
                std::move(app_profile_id), std::move(options), Config{});
}

std::shared_ptr<ClientConfigurationManager> ClientConfigurationManager::Create(
    CompletionQueue cq, std::shared_ptr<BigtableStub> stub,
    std::string instance_name, std::string app_profile_id, Options options,
    Config config) {
  return Create(std::move(cq), std::move(stub), std::move(instance_name),
                std::move(app_profile_id), std::move(options),
                std::move(config), std::make_shared<SteadyClock>(),
                std::make_shared<SystemClock>());
}

std::shared_ptr<ClientConfigurationManager> ClientConfigurationManager::Create(
    CompletionQueue cq, std::shared_ptr<BigtableStub> stub,
    std::string instance_name, std::string app_profile_id, Options options,
    Config config, std::shared_ptr<SteadyClock> steady_clock,
    std::shared_ptr<SystemClock> system_clock) {
  return std::shared_ptr<ClientConfigurationManager>(
      new ClientConfigurationManager(
          std::move(cq), std::move(stub), std::move(instance_name),
          std::move(app_profile_id), std::move(options), std::move(config),
          std::move(steady_clock), std::move(system_clock)));
}

ClientConfigurationManager::ClientConfigurationManager(
    CompletionQueue cq, std::shared_ptr<BigtableStub> stub,
    std::string instance_name, std::string app_profile_id, Options options,
    Config config, std::shared_ptr<SteadyClock> steady_clock,
    std::shared_ptr<SystemClock> system_clock)
    : cq_(std::move(cq)),
      stub_(std::move(stub)),
      instance_name_(std::move(instance_name)),
      app_profile_id_(std::move(app_profile_id)),
      options_(internal::MakeImmutableOptions(
          SanitizeConfigPollOptions(std::move(options)))),
      config_(std::move(config)),
      steady_clock_(std::move(steady_clock)),
      system_clock_(std::move(system_clock)),
      prng_(internal::MakeDefaultPRNG()) {
  if (!config_.override_config.has_value()) {
    config_.override_config = LoadEnvOverrideConfiguration();
  }
  current_config_ = ApplyOverrides(config_.default_config);
  valid_until_ =
      steady_clock_->Now() + EffectiveValidityDuration(current_config_);
}

ClientConfigurationManager::~ClientConfigurationManager() { Shutdown(); }

google::bigtable::v2::ClientConfiguration
ClientConfigurationManager::ApplyOverrides(
    google::bigtable::v2::ClientConfiguration cfg) const {
  if (config_.override_config.has_value()) {
    cfg.MergeFrom(*config_.override_config);
  }
  return cfg;
}

void ClientConfigurationManager::Start() {
  std::unique_lock<std::mutex> lock(mu_);
  if (started_ || shutdown_ || stub_ == nullptr) return;
  started_ = true;
  StartFetchAttempt(lock, /*attempt=*/0, steady_clock_->Now());
}

void ClientConfigurationManager::Shutdown() {
  shutdown_flag_.store(true, std::memory_order_release);

  future<void> timer_to_cancel;
  std::shared_ptr<grpc::ClientContext> context_to_cancel;
  std::vector<std::shared_ptr<ListenerSlot>> slots;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (shutdown_) return;
    shutdown_ = true;
    ++timer_generation_;
    ++op_generation_;
    timer_to_cancel = std::move(timer_);
    context_to_cancel = std::move(active_rpc_context_);
    slots.reserve(listeners_.size());
    for (auto& [id, slot] : listeners_) {
      slot->active.store(false, std::memory_order_release);
      slots.push_back(std::move(slot));
    }
    listeners_.clear();
  }

  if (context_to_cancel != nullptr) context_to_cancel->TryCancel();
  if (timer_to_cancel.valid()) timer_to_cancel.cancel();

  std::thread::id const self_thread = std::this_thread::get_id();
  for (std::shared_ptr<ListenerSlot> const& slot : slots) {
    if (slot->delivering_thread.load(std::memory_order_acquire) ==
        self_thread) {
      continue;
    }
    std::lock_guard<std::mutex> delivery_lock(slot->delivery_mu);
  }
}

google::bigtable::v2::ClientConfiguration
ClientConfigurationManager::GetConfig() const {
  std::lock_guard<std::mutex> lock(mu_);
  return current_config_;
}

std::int64_t ClientConfigurationManager::config_seq() const {
  std::lock_guard<std::mutex> lock(mu_);
  return config_seq_;
}

ClientConfigurationManager::Snapshot ClientConfigurationManager::GetSnapshot()
    const {
  std::lock_guard<std::mutex> lock(mu_);
  Snapshot snap;
  snap.instance_name = instance_name_;
  snap.app_profile_id = app_profile_id_;
  snap.config_seq = config_seq_;
  snap.valid_until = valid_until_;
  snap.last_response = last_response_;
  snap.last_fetched_at = last_fetched_at_;
  snap.last_error = last_error_;
  snap.last_error_at = last_error_at_;
  snap.poll_history.assign(poll_history_.begin(), poll_history_.end());
  snap.captured_at = system_clock_->Now();
  return snap;
}

ClientConfigurationManager::UnregisterCallback
ClientConfigurationManager::AddListener(ConfigListener listener) {
  return AddListener(std::move(listener), /*fire_immediately=*/true);
}

ClientConfigurationManager::UnregisterCallback
ClientConfigurationManager::AddListener(ConfigListener listener,
                                        bool fire_immediately) {
  if (!listener) return [] {};

  std::shared_ptr<ListenerSlot> slot;
  std::uint64_t id = 0;
  google::bigtable::v2::ClientConfiguration initial_config;
  std::int64_t initial_seq = 0;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (shutdown_) return [] {};
    id = ++next_listener_id_;
    slot = std::make_shared<ListenerSlot>(std::move(listener));
    listeners_.emplace(id, slot);
    if (fire_immediately) {
      initial_config = current_config_;
      initial_seq = config_seq_;
    }
  }

  if (fire_immediately) DeliverToSlot(*slot, initial_config, initial_seq);

  std::weak_ptr<ClientConfigurationManager> weak_self = weak_from_this();
  std::weak_ptr<ListenerSlot> weak_slot = slot;
  return [weak_self = std::move(weak_self), weak_slot = std::move(weak_slot),
          id]() {
    if (std::shared_ptr<ListenerSlot> s = weak_slot.lock()) {
      s->active.store(false, std::memory_order_release);
    }
    if (std::shared_ptr<ClientConfigurationManager> self = weak_self.lock()) {
      std::lock_guard<std::mutex> lock(self->mu_);
      self->listeners_.erase(id);
    }
  };
}

ClientConfigurationManager::UnregisterCallback
ClientConfigurationManager::AddSessionPoolConfigurationListener(
    SessionPoolConfigurationListener listener) {
  if (!listener) return [] {};

  struct DiffState {
    bool has_prev = false;
    google::bigtable::v2::SessionClientConfiguration::SessionPoolConfiguration
        prev;
  };
  auto state = std::make_shared<DiffState>();
  google::bigtable::v2::SessionClientConfiguration::SessionPoolConfiguration
      default_pool_cfg = config_.default_config.session_configuration()
                             .session_pool_configuration();

  return AddListener(
      [listener = std::move(listener), state = std::move(state),
       default_pool_cfg = std::move(default_pool_cfg)](
          google::bigtable::v2::ClientConfiguration const& cfg, std::int64_t) {
        google::bigtable::v2::SessionClientConfiguration::
            SessionPoolConfiguration const& pool_cfg =
                cfg.has_session_configuration() &&
                        cfg.session_configuration()
                            .has_session_pool_configuration()
                    ? cfg.session_configuration().session_pool_configuration()
                    : default_pool_cfg;
        if (state->has_prev &&
            google::protobuf::util::MessageDifferencer::Equals(state->prev,
                                                               pool_cfg)) {
          return;
        }
        state->has_prev = true;
        state->prev = pool_cfg;
        listener(pool_cfg);
      },
      /*fire_immediately=*/true);
}

ClientConfigurationManager::UnregisterCallback
ClientConfigurationManager::AddDiversionLoadsListener(
    DiversionLoadsListener listener) {
  if (!listener) return [] {};

  struct DiffState {
    bool has_prev = false;
    DiversionLoads prev;
  };
  auto state = std::make_shared<DiffState>();

  return AddListener(
      [listener = std::move(listener), state = std::move(state)](
          google::bigtable::v2::ClientConfiguration const& cfg,
          std::int64_t seq) {
        if (seq == 0) return;
        DiversionLoads const loads =
            cfg.has_session_configuration()
                ? ResolveDiversionLoads(cfg.session_configuration())
                : DiversionLoads{};
        if (state->has_prev && state->prev == loads) return;
        state->has_prev = true;
        state->prev = loads;
        listener(loads);
      },
      /*fire_immediately=*/false);
}

void ClientConfigurationManager::StartFetchAttempt(
    std::unique_lock<std::mutex>& lock, int attempt,
    std::chrono::steady_clock::time_point poll_start) {
  if (shutdown_ || stub_ == nullptr) return;

  std::uint64_t const op_id = ++op_generation_;
  auto context = std::make_shared<grpc::ClientContext>();
  internal::ConfigureContext(*context, *options_);
  context->set_deadline(system_clock_->Now() + config_.poll_deadline);

  std::vector<std::string> request_params;
  request_params.reserve(2);
  if (!instance_name_.empty()) {
    context->AddMetadata("google-cloud-resource-prefix", instance_name_);
    request_params.push_back(
        absl::StrCat("instance_name=", internal::UrlEncode(instance_name_)));
  }
  if (!app_profile_id_.empty()) {
    request_params.push_back(
        absl::StrCat("app_profile_id=", internal::UrlEncode(app_profile_id_)));
  }
  if (!request_params.empty()) {
    context->AddMetadata("x-goog-request-params",
                         absl::StrJoin(request_params, "&"));
  }

  google::bigtable::v2::GetClientConfigurationRequest request;
  request.set_instance_name(instance_name_);
  request.set_app_profile_id(app_profile_id_);

  active_rpc_context_ = context;
  std::shared_ptr<BigtableStub> stub = stub_;
  CompletionQueue cq = cq_;
  internal::ImmutableOptions options = options_;
  auto op_ctx = std::make_shared<OperationContext>();
  std::weak_ptr<ClientConfigurationManager> weak_self = weak_from_this();
  lock.unlock();

  internal::OptionsSpan span(options);
  future<StatusOr<google::bigtable::v2::ClientConfiguration>> rpc_future =
      stub->AsyncGetClientConfiguration(cq, context, options, request,
                                        std::move(op_ctx));
  if (!rpc_future.valid()) {
    OnFetchAttemptCompleted(
        op_id, attempt, poll_start,
        Status(StatusCode::kUnimplemented,
               "AsyncGetClientConfiguration returned an invalid future"));
    lock.lock();
    return;
  }
  std::move(rpc_future)
      .then([weak_self = std::move(weak_self), options, op_id, attempt,
             poll_start](
                future<StatusOr<google::bigtable::v2::ClientConfiguration>> f) {
        internal::OptionsSpan cb_span(options);
        if (std::shared_ptr<ClientConfigurationManager> self =
                weak_self.lock()) {
          self->OnFetchAttemptCompleted(op_id, attempt, poll_start, f.get());
        }
      });

  lock.lock();
  if (shutdown_) {
    lock.unlock();
    context->TryCancel();
    lock.lock();
  }
}

void ClientConfigurationManager::OnFetchAttemptCompleted(
    std::uint64_t op_id, int attempt,
    std::chrono::steady_clock::time_point poll_start,
    StatusOr<google::bigtable::v2::ClientConfiguration> result) {
  std::optional<PendingNotification> notification;
  {
    std::unique_lock<std::mutex> lock(mu_);
    if (shutdown_ || op_id != op_generation_) return;
    ++op_generation_;
    active_rpc_context_.reset();

    if (result.ok()) {
      notification = HandleFetchSuccess(lock, poll_start, *std::move(result));
    } else {
      notification = HandleFetchFailure(lock, attempt, poll_start,
                                        std::move(result).status());
    }
  }

  if (!notification.has_value() ||
      shutdown_flag_.load(std::memory_order_acquire)) {
    return;
  }
  for (std::shared_ptr<ListenerSlot> const& slot : notification->listeners) {
    DeliverToSlot(*slot, notification->config, notification->seq);
  }
}

std::optional<ClientConfigurationManager::PendingNotification>
ClientConfigurationManager::HandleFetchSuccess(
    std::unique_lock<std::mutex>& lock,
    std::chrono::steady_clock::time_point poll_start,
    google::bigtable::v2::ClientConfiguration response) {
  std::chrono::steady_clock::time_point const now_steady = steady_clock_->Now();
  std::chrono::system_clock::time_point const now_sys = system_clock_->Now();
  last_response_ = response;
  last_fetched_at_ = now_sys;
  last_error_.reset();
  last_error_at_.reset();

  google::bigtable::v2::ClientConfiguration effective =
      ApplyOverrides(std::move(response));
  valid_until_ = now_steady + (effective.stop_polling()
                                   ? kMaxValidityDuration
                                   : EffectiveValidityDuration(effective));

  std::optional<PendingNotification> notification;
  if (!google::protobuf::util::MessageDifferencer::Equals(effective,
                                                          current_config_)) {
    current_config_ = std::move(effective);
    notification = CaptureNotificationLocked();
  }

  std::chrono::milliseconds const elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(now_steady -
                                                            poll_start);
  RecordPollLocked(PollEvent{now_sys, elapsed, Status{}, config_seq_});
  ScheduleNextPoll(lock);
  return notification;
}

std::optional<ClientConfigurationManager::PendingNotification>
ClientConfigurationManager::HandleFetchFailure(
    std::unique_lock<std::mutex>& lock, int attempt,
    std::chrono::steady_clock::time_point poll_start, Status status) {
  int const max_retries = EffectiveMaxRpcRetryCount(current_config_);
  if (IsRetryableConfigStatus(status.code()) && attempt < max_retries) {
    ScheduleRetry(lock, attempt + 1, poll_start);
    return std::nullopt;
  }

  std::chrono::steady_clock::time_point const now_steady = steady_clock_->Now();
  std::chrono::system_clock::time_point const now_sys = system_clock_->Now();
  std::chrono::milliseconds const elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(now_steady -
                                                            poll_start);
  last_error_ = status;
  last_error_at_ = now_sys;

  std::optional<PendingNotification> notification;
  if (now_steady >= valid_until_) {
    current_config_ = ApplyOverrides(config_.default_config);
    valid_until_ = now_steady + EffectiveValidityDuration(current_config_);
    notification = CaptureNotificationLocked();
  }
  RecordPollLocked(PollEvent{now_sys, elapsed, std::move(status), config_seq_});
  ScheduleNextPoll(lock);
  return notification;
}

ClientConfigurationManager::PendingNotification
ClientConfigurationManager::CaptureNotificationLocked() {
  PendingNotification pending;
  pending.seq = ++config_seq_;
  pending.config = current_config_;
  pending.listeners.reserve(listeners_.size());
  for (auto const& [id, slot] : listeners_) {
    pending.listeners.push_back(slot);
  }
  return pending;
}

void ClientConfigurationManager::ScheduleRetry(
    std::unique_lock<std::mutex>& lock, int next_attempt,
    std::chrono::steady_clock::time_point poll_start) {
  std::chrono::milliseconds const delay = ComputeRetryBackoff(next_attempt - 1);
  ScheduleTimer(lock, delay,
                [this, next_attempt,
                 poll_start](std::unique_lock<std::mutex>& inner_lock) {
                  StartFetchAttempt(inner_lock, next_attempt, poll_start);
                });
}

void ClientConfigurationManager::ScheduleNextPoll(
    std::unique_lock<std::mutex>& lock) {
  if (shutdown_ || current_config_.stop_polling()) return;
  std::chrono::milliseconds const delay =
      EffectivePollingInterval(current_config_, config_.min_polling_interval);
  ScheduleTimer(lock, delay, [this](std::unique_lock<std::mutex>& inner_lock) {
    StartFetchAttempt(inner_lock, /*attempt=*/0, steady_clock_->Now());
  });
}

void ClientConfigurationManager::ScheduleTimer(
    std::unique_lock<std::mutex>& lock, std::chrono::milliseconds delay,
    std::function<void(std::unique_lock<std::mutex>&)> action) {
  if (shutdown_) return;

  future<void> prev_timer = std::move(timer_);
  std::uint64_t const timer_id = ++timer_generation_;
  std::weak_ptr<ClientConfigurationManager> weak_self = weak_from_this();
  CompletionQueue cq = cq_;
  internal::ImmutableOptions options = options_;
  lock.unlock();

  if (prev_timer.valid()) prev_timer.cancel();

  future<void> next_timer = cq.MakeRelativeTimer(delay).then(
      [weak_self = std::move(weak_self), options, timer_id,
       action = std::move(action)](
          future<StatusOr<std::chrono::system_clock::time_point>> fut) {
        internal::OptionsSpan span(options);
        if (!fut.get().ok()) return;
        if (std::shared_ptr<ClientConfigurationManager> self =
                weak_self.lock()) {
          std::unique_lock<std::mutex> inner_lock(self->mu_);
          if (self->shutdown_ || self->timer_generation_ != timer_id) {
            return;
          }
          self->timer_ = {};
          action(inner_lock);
        }
      });

  lock.lock();
  if (!shutdown_ && timer_generation_ == timer_id) {
    timer_ = std::move(next_timer);
    return;
  }
  lock.unlock();
  if (next_timer.valid()) next_timer.cancel();
  lock.lock();
}

std::chrono::milliseconds ClientConfigurationManager::ComputeRetryBackoff(
    int attempt) {
  int const clamped_shift = std::clamp(attempt, 0, 20);
  std::int64_t const base_ms =
      (std::max)(std::int64_t{0}, static_cast<std::int64_t>(
                                      config_.initial_retry_backoff.count()));
  std::int64_t const max_ms =
      (std::max)(base_ms,
                 static_cast<std::int64_t>(config_.max_retry_backoff.count()));
  std::int64_t limit_ms = max_ms;
  if (base_ms == 0) {
    limit_ms = 0;
  } else if (base_ms <= max_ms >> clamped_shift) {
    limit_ms = base_ms << clamped_shift;
  }
  if (limit_ms <= 0) return std::chrono::milliseconds(0);
  std::uniform_int_distribution<std::int64_t> dist(0, limit_ms);
  return std::chrono::milliseconds(dist(prng_));
}

void ClientConfigurationManager::RecordPollLocked(PollEvent event) {
  if (poll_history_.size() >= kMaxPollHistory) poll_history_.pop_front();
  poll_history_.push_back(std::move(event));
}

void ClientConfigurationManager::DeliverToSlot(
    ListenerSlot& slot, google::bigtable::v2::ClientConfiguration const& config,
    std::int64_t seq) const {
  std::lock_guard<std::mutex> delivery_lock(slot.delivery_mu);
  if (!slot.active.load(std::memory_order_acquire) ||
      shutdown_flag_.load(std::memory_order_acquire)) {
    return;
  }
  if (seq <= slot.last_delivered_seq) return;
  slot.last_delivered_seq = seq;
  slot.delivering_thread.store(std::this_thread::get_id(),
                               std::memory_order_release);
  struct ClearThreadGuard {
    std::atomic<std::thread::id>& ref;
    ~ClearThreadGuard() {
      ref.store(std::thread::id{}, std::memory_order_release);
    }
  } guard{slot.delivering_thread};
  slot.callback(config, seq);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
