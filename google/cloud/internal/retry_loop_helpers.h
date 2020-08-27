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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RETRY_LOOP_HELPERS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RETRY_LOOP_HELPERS_H

#include "google/cloud/status_or.h"
#include "google/cloud/version.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

/// Generic programming adapter for `RetryLoop()` and `AsyncRetryLoop()`.
inline Status GetResultStatus(Status status) { return status; }

/// @copydoc GetResultStatus(Status)
template <typename T>
Status GetResultStatus(StatusOr<T> result) {
  return std::move(result).status();
}

/// Generate an error Status for `RetryLoop()` and `AsyncRetryLoop()`
Status RetryLoopError(char const* loop_message, char const* location,
                      Status const& last_status);

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RETRY_LOOP_HELPERS_H
