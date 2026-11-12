// Copyright 2026 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_CANCELLATION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_CANCELLATION_H

#include "google/cloud/internal/make_status.h"
#include "google/cloud/rest_options.h"
#include <algorithm>
#include <chrono>
#include <thread>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

inline bool IsRestCancelled(Options const& options) {
  return options.has<experimental::RestCancellationOption>() &&
         options.get<experimental::RestCancellationOption>().IsCancelled();
}

inline Status RestCancelledError() {
  return internal::CancelledError("REST operation cancelled", GCP_ERROR_INFO());
}

// Preserve the usual sleep for uncancellable calls. For cancellable calls the
// same bounded polling as the transport avoids callbacks and lifetime coupling.
inline void RestCancellationWait(Options const& options,
                                 std::chrono::milliseconds delay) {
  if (!options.has<experimental::RestCancellationOption>()) {
    std::this_thread::sleep_for(delay);
    return;
  }
  auto const deadline = std::chrono::steady_clock::now() + delay;
  while (!IsRestCancelled(options)) {
    auto const remaining = deadline - std::chrono::steady_clock::now();
    if (remaining <= decltype(remaining)::zero()) return;
    std::this_thread::sleep_for(
        (std::min)(remaining, std::chrono::steady_clock::duration(
                                  std::chrono::milliseconds(50))));
  }
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_CANCELLATION_H
