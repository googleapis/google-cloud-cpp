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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_ALARM_REGISTRY_INTERFACE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_ALARM_REGISTRY_INTERFACE_H

#include "google/cloud/version.h"
#include <chrono>
#include <functional>
#include <memory>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

class AlarmRegistryInterface {
 protected:
  AlarmRegistryInterface() = default;

 public:
  virtual ~AlarmRegistryInterface() = default;
  AlarmRegistryInterface(const AlarmRegistryInterface&) = delete;
  AlarmRegistryInterface& operator=(const AlarmRegistryInterface&) = delete;
  AlarmRegistryInterface(AlarmRegistryInterface&&) = delete;
  AlarmRegistryInterface& operator=(AlarmRegistryInterface&&) = delete;
  // When CancelToken is destroyed, the alarm will not be running and will never
  // run again.
  class CancelToken {
   protected:
    CancelToken() = default;

   public:
    virtual ~CancelToken() = default;
    CancelToken(const CancelToken&) = delete;
    CancelToken& operator=(const CancelToken&) = delete;
    CancelToken(AlarmRegistryInterface&&) = delete;
    CancelToken& operator=(CancelToken&&) = delete;
  };

  // Register an alarm to run with a given period.  It is guaranteed not to run
  // inline.
  virtual std::unique_ptr<CancelToken> RegisterAlarm(
      std::chrono::milliseconds period, std::function<void()> on_alarm) = 0;
};

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_ALARM_REGISTRY_INTERFACE_H
