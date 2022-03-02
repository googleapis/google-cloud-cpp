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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_BATCHING_SETTINGS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_BATCHING_SETTINGS_H

#include "google/cloud/version.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

class BatchingSettings {
 public:
  BatchingSettings(size_t byte_limit, size_t message_limit)
      : byte_limit_{byte_limit}, message_limit_{message_limit} {}

  BatchingSettings(const BatchingSettings&) = default;
  BatchingSettings& operator=(const BatchingSettings&) = default;
  BatchingSettings(BatchingSettings&&) = default;
  BatchingSettings& operator=(BatchingSettings&&) = default;

  size_t GetByteLimit() const { return byte_limit_; }
  size_t GetMessageLimit() const { return message_limit_; }

 private:
  size_t byte_limit_;
  size_t message_limit_;
};

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_BATCHING_SETTINGS_H
