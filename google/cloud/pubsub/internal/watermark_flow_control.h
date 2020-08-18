// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_WATERMARK_FLOW_CONTROL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_WATERMARK_FLOW_CONTROL_H

#include "google/cloud/pubsub/version.h"
#include <mutex>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * Implement a simple admission control check.
 *
 * This class is used in the implementation of flow control
 */
class WatermarkFlowControl {
 public:
  WatermarkFlowControl(std::size_t lwm, std::size_t hwm)
      : lwm_(lwm), hwm_(hwm) {}

  /// Admit some work if there is capacity, returns true if admitted.
  bool MaybeAdmit(std::size_t size);

  /// Release some work, returns true if more work can be scheduled.
  bool Release(std::size_t size);

 private:
  std::size_t const lwm_;
  std::size_t const hwm_;
  std::mutex mu_;
  std::size_t current_ = 0;
  bool overflow_ = false;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_WATERMARK_FLOW_CONTROL_H
