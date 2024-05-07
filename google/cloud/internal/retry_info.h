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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RETRY_INFO_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RETRY_INFO_H

#include "google/cloud/version.h"
#include <chrono>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * Recommendation for backoff from the server.
 *
 * This class is internal, because it is only relevant to the client library.
 *
 * @see https://cloud.google.com/apis/design/errors#retry_info
 */
class RetryInfo {
 public:
  explicit RetryInfo(std::chrono::nanoseconds retry_delay)
      : retry_delay_(retry_delay) {}

  std::chrono::nanoseconds retry_delay() const { return retry_delay_; }

  friend bool operator==(RetryInfo const& a, RetryInfo const& b) {
    return a.retry_delay_ == b.retry_delay_;
  }

  friend bool operator!=(RetryInfo const& a, RetryInfo const& b) {
    return !(a == b);
  }

 private:
  std::chrono::nanoseconds retry_delay_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RETRY_INFO_H
