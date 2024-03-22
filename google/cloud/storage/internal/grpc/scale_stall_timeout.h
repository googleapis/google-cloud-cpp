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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_SCALE_STALL_TIMEOUT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_SCALE_STALL_TIMEOUT_H

#include "google/cloud/version.h"
#include <chrono>
#include <cstddef>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Convert stall timeout parameters to a per-message timeout.
 *
 * The storage library does not define "total time" to timeout uploads and
 * downloads. The transfers, and their total time, can vary by 12 orders of
 * magnitude, from transferring basically empty objects to transferring 5 TiB
 * objects.
 *
 * Instead the library tries to restart transfers that show "lack of progress"
 * or "stall". Applications configure the storage library to detect stalled
 * uploads and downloads using two parameters:
 * - A time duration, expressed in seconds
 * - The minimum number of bytes expected to be transferred in that duration.
 *
 * A time duration of zero seconds disables this feature.
 *
 * This approach does not work well with gRPC-based transfers, where the
 * transfers are broken up into a series of messages. In the current
 * implementation we use a per-message timeout. The maximum message size is
 * known: it is part of the public contract in the service. So we can scale the
 * timeout based on this parameter.
 */
std::chrono::milliseconds ScaleStallTimeout(std::chrono::seconds stall_duration,
                                            std::size_t stall_size,
                                            std::size_t maximum_message_size);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_SCALE_STALL_TIMEOUT_H
