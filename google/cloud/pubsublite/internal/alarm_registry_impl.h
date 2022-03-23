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

  // TODO(18suresha): implement destructor sanity check?
  //  ~AlarmRegistryImpl() override;
  // When CancelToken is destroyed, the alarm will not be running and will never
  // run again.
  class CancelTokenImpl : public AlarmRegistry::CancelToken {
   public:
    CancelTokenImpl(std::shared_ptr<std::mutex> mu,
                    std::shared_ptr<bool> shutdown);

    ~CancelTokenImpl() override;

   private:
    std::shared_ptr<std::mutex> mu_;
    std::shared_ptr<bool> shutdown_;  // ABSL_GUARDED_BY(*mu_)
  };

  std::unique_ptr<AlarmRegistry::CancelToken> RegisterAlarm(
      std::chrono::milliseconds period,
      std::function<void()> on_alarm) override;

 private:
  struct AlarmState {
    CompletionQueue cq;
    std::chrono::milliseconds period;
    std::shared_ptr<std::mutex> mu;
    // make on_alarm unique_ptr/shared_ptr?
    std::function<void()> on_alarm;  // ABSL_GUARDED_BY(*mu_)
    std::shared_ptr<bool> shutdown;  // ABSL_GUARDED_BY(*mu_)
  };

  // static with arguments rather than member variables, so parameters aren't
  // bound to object lifetime
  static void OnAlarm(std::shared_ptr<AlarmState> const& state);

  google::cloud::CompletionQueue cq_;
};

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_ALARM_REGISTRY_IMPL_H
