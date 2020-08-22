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
 * This class is used in the implementation of flow control. It admits work
 * until *either* the high-watermark for size or count is reached, and then
 * rejects work until *both* the low-watermark for size and count are cleared.
 */
class WatermarkFlowControl {
 public:
  WatermarkFlowControl(std::size_t count_lwm, std::size_t count_hwm,
                       std::size_t size_lwm, std::size_t size_hwm)
      : count_lwm_(count_lwm),
        count_hwm_(count_hwm),
        size_lwm_(size_lwm),
        size_hwm_(size_hwm) {}

  /// Admit some work if there is capacity, returns true if admitted.
  bool MaybeAdmit(std::size_t size);

  /// Release some work, returns true if more work can be scheduled.
  bool Release(std::size_t size);

 private:
  bool IsFull() const {
    return current_count_ >= count_hwm_ || current_size_ >= size_hwm_;
  }

  bool HasCapacity() const {
    return current_count_ <= count_lwm_ && current_size_ <= size_lwm_;
  }

  std::size_t const count_lwm_;
  std::size_t const count_hwm_;
  std::size_t const size_lwm_;
  std::size_t const size_hwm_;
  std::mutex mu_;
  std::size_t current_count_ = 0;
  std::size_t current_size_ = 0;
  bool overflow_ = false;
};

/**
 * Implement a simple admission control check.
 *
 * This class is used in the implementation of flow control
 */
class WatermarkFlowControlCountOnly {
 public:
  WatermarkFlowControlCountOnly(std::size_t count_lwm, std::size_t count_hwm)
      : flow_control_(count_lwm, count_hwm, count_lwm, count_hwm) {}

  /// Admit some work if there is capacity, returns true if admitted.
  bool MaybeAdmit() { return flow_control_.MaybeAdmit(1); }

  /// Release some work, returns true if more work can be scheduled.
  bool Release() { return flow_control_.Release(1); }

 private:
  WatermarkFlowControl flow_control_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_WATERMARK_FLOW_CONTROL_H
