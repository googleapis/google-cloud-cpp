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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_ALARM_REGISTRY_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_ALARM_REGISTRY_IMPL_H

#include "google/cloud/pubsublite/internal/alarm_registry.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/version.h"
#include <deque>
#include <mutex>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

// TODO(18suresha): description
class AlarmRegistryImpl : public AlarmRegistry {
 public:
  explicit AlarmRegistryImpl(google::cloud::CompletionQueue cq);

  std::unique_ptr<AlarmRegistry::CancelToken> RegisterAlarm(
      std::chrono::milliseconds period,
      std::function<void()> on_alarm) override;

 private:
  struct AlarmState {
    AlarmState(CompletionQueue completion_queue,
               std::chrono::milliseconds period,
               std::function<void()> alarm_func)
        : cq{std::move(completion_queue)},
          period{period},
          on_alarm{std::move(alarm_func)} {}
    CompletionQueue cq;
    std::chrono::milliseconds const period;
    std::mutex mu;
    std::function<void()> on_alarm;  // ABSL_GUARDED_BY(mu)
    bool shutdown = false;           // ABSL_GUARDED_BY(mu)
  };

  // When CancelToken is destroyed, the alarm will not be running and will never
  // run again.
  class CancelTokenImpl : public AlarmRegistry::CancelToken {
   public:
    explicit CancelTokenImpl(std::shared_ptr<AlarmState>);

    ~CancelTokenImpl() override;

   private:
    std::shared_ptr<AlarmState> state_;
  };

  // static with arguments rather than member variables, so parameters aren't
  // bound to object lifetime
  static void ScheduleAlarm(std::shared_ptr<AlarmState> const& state);

  google::cloud::CompletionQueue const cq_;
};

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_ALARM_REGISTRY_IMPL_H
