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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ALARM_REGISTRY_INTERFACE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ALARM_REGISTRY_INTERFACE_H

#include "google/cloud/version.h"
#include "google/cloud/internal/base_interface.h"
#include "absl/time/time.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

class AlarmRegistryInterface : public BaseInterface {
 public:
  // When CancelToken is destroyed, the alarm will not be running and will never
  // run again.
  class CancelToken : public BaseInterface {};

  // Register an alarm to run with a given period.  It is guaranteed not to run
  // inline.
  virtual std::unique_ptr<CancelToken> RegisterAlarm(
      absl::Duration period, std::function<void()> on_alarm) = 0;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ALARM_REGISTRY_INTERFACE_H
