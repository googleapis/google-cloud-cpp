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
#include "google/cloud/log.h"
#include "google/cloud/version.h"
#include "absl/memory/memory.h"
#include <utility>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

AlarmRegistryImpl::AlarmRegistryImpl(google::cloud::CompletionQueue cq)
    : cq_{std::move(cq)} {}

void AlarmRegistryImpl::ScheduleAlarm(
    std::shared_ptr<AlarmState> const& state) {
  auto temp =
      state->cq.MakeRelativeTimer(state->period)
          .then([state](
                    future<StatusOr<std::chrono::system_clock::time_point>> f) {
            auto status = f.get();
            if (!status.ok()) {
              GCP_LOG(INFO) << "`MakeRelativeTimer` returned a non-ok status: "
                            << status.status();
              return;
            }
            {
              std::lock_guard<std::mutex> g{state->mu};
              if (state->shutdown) return;
              state->on_alarm();
            }
            ScheduleAlarm(state);
          });
  std::lock_guard<std::mutex> g{state->mu};
  state->timer = std::move(temp);
}

AlarmRegistryImpl::CancelTokenImpl::CancelTokenImpl(
    std::shared_ptr<AlarmState> state)
    : state_{std::move(state)} {}

AlarmRegistryImpl::CancelTokenImpl::~CancelTokenImpl() {
  // the alarm function is guarded by mu and is only invoked after checking
  // shutdown all while guarded by mu, so this guarantees that the alarm
  // function isn't running when the destructor is run and the function won't
  // run after the destructor finishes
  std::lock_guard<std::mutex> g{state_->mu};
  state_->timer.cancel();
  state_->shutdown = true;
}

std::unique_ptr<AlarmRegistry::CancelToken> AlarmRegistryImpl::RegisterAlarm(
    std::chrono::milliseconds period, std::function<void()> on_alarm) {
  auto state = std::make_shared<AlarmState>(cq_, period, std::move(on_alarm));
  ScheduleAlarm(state);
  return absl::make_unique<CancelTokenImpl>(state);
}

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
