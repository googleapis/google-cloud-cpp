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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_BATCHING_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_BATCHING_OPTIONS_H

#include "google/cloud/version.h"
#include <algorithm>
#include <chrono>

namespace google {
namespace cloud {
namespace pubsublite {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Batching options for a `Publisher`.
 */
class BatchingOptions {
 public:
  size_t maximum_batch_message_count() const { return max_batch_messages_; }

  /**
   * Set the maximum number of messages in a batch.
   *
   * @note Application developers should keep in mind that Cloud Pub/Sub Lite
   *    sets limits on the size of a batch (1,000 messages)
   *    The library truncates the argument to 1,000 if invalid.
   */
  void set_maximum_batch_message_count(size_t v) {
    max_batch_messages_ = std::min<size_t>(v, 1000);
  }

  size_t maximum_batch_bytes() const { return max_batch_bytes_; }

  /**
   * Set the maximum size for the messages in a batch.
   *
   * @note Application developers should keep in mind that Cloud Pub/Sub Lite
   *    sets limits on the size of a batch (3.5MiB). The
   *    library truncates the argument to 3.5 * 1024 * 1024 if invalid.
   */
  void set_maximum_batch_bytes(size_t v) {
    max_batch_bytes_ = std::min<size_t>(v, 1024 * 1024 * 7 / 2);
  }

  std::chrono::milliseconds alarm_period() const { return alarm_period_; }

  /**
   * Set the frequency of when `PublishRequest`s should be sent.
   */
  void set_alarm_period(std::chrono::milliseconds v) { alarm_period_ = v; }

 private:
  size_t max_batch_messages_{1000};
  size_t max_batch_bytes_{1024 * 1024 * 7 / 2};
  std::chrono::milliseconds alarm_period_{10};
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_BATCHING_OPTIONS_H
