// Copyright 2024 Google LLC
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

#include "google/cloud/storage/internal/grpc/scale_stall_timeout.h"
#include <chrono>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::chrono::milliseconds ScaleStallTimeout(std::chrono::seconds stall_duration,
                                            std::size_t stall_size,
                                            std::size_t maximum_message_size) {
  using ms = std::chrono::milliseconds;
  if (stall_duration == ms(0)) return ms(0);
  if (stall_size <= maximum_message_size || stall_size == 0) {
    return ms(stall_duration);
  }
  // In practice this should not overflow. The current value for
  // `maximum_message_size` is 2MiB, that is 21 bits. Meanwhile
  // `std::chrono::milliseconds::Rep` is guaranteed to have at least 45 bits,
  // and in practice it is 64-bits. Even in the 45-bit case this should support
  // `stall_duration <= 4.6 hours`. Like 640KiB, that should be enough for
  // everybody. In practice, with 64-bit `Rep`, this supports values of
  // `stall_duration` longer than 1,000 years.
  return ms(stall_duration) * maximum_message_size / stall_size;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
