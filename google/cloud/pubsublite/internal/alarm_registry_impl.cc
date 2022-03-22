// Copyright 2022 Google LLC
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

#include "google/cloud/pubsublite/internal/alarm_registry_impl.h"
#include "google/cloud/version.h"
#include "absl/memory/memory.h"
#include <utility>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

AlarmRegistryImpl::AlarmRegistryImpl(google::cloud::CompletionQueue cq)
    : cq_{std::move(cq)} {}

void AlarmRegistryImpl::OnAlarm(std::uint64_t i,
                                std::shared_ptr<bool> const& alarm_status,
                                std::shared_ptr<std::mutex> const& mu) {
  {
    std::lock_guard<std::mutex> g{*mu};
    if (!*alarm_status) return;
  }
  cq_.MakeRelativeTimer(registered_alarms_[i].period)
      .then([this, i, alarm_status,
             mu](future<StatusOr<std::chrono::system_clock::time_point>> f) {
        bool is_ok = f.get().ok();
        {
          std::lock_guard<std::mutex> g{*mu};
          if (!*alarm_status) return;
          if (!is_ok) {
            *alarm_status = false;
            return;
          }
          registered_alarms_[i].on_alarm();
        }
        OnAlarm(i, std::move(alarm_status), std::move(mu));
      });
}

AlarmRegistryImpl::CancelTokenImpl::CancelTokenImpl(
    std::shared_ptr<bool> alarm_status, std::shared_ptr<std::mutex> mu)
    : alarm_status_{std::move(alarm_status)}, mu_{std::move(mu)} {}

AlarmRegistryImpl::CancelTokenImpl::~CancelTokenImpl() {
  std::lock_guard<std::mutex> g{*mu_};
  *alarm_status_ = false;
}

std::unique_ptr<AlarmRegistry::CancelToken> AlarmRegistryImpl::RegisterAlarm(
    std::chrono::milliseconds period, std::function<void()> on_alarm) {
  auto last_index = registered_alarms_.size();
  registered_alarms_.push_back(
      RegisteredAlarmData{period, std::move(on_alarm)});
  // mu guards status
  auto status = std::make_shared<bool>(true);
  auto mu = std::make_shared<std::mutex>();
  std::unique_ptr<AlarmRegistry::CancelToken> cancel_token =
      absl::make_unique<CancelTokenImpl>(status, mu);
  OnAlarm(last_index, status, mu);
  return cancel_token;
}

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
