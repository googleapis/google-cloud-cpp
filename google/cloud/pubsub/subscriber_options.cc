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

#include "google/cloud/pubsub/subscriber_options.h"
#include <algorithm>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

SubscriberOptions& SubscriberOptions::set_max_outstanding_messages(
    std::int64_t message_count) {
  max_outstanding_messages_ = (std::max<std::int64_t>)(0, message_count);
  return *this;
}

SubscriberOptions& SubscriberOptions::set_max_outstanding_bytes(
    std::int64_t bytes) {
  max_outstanding_bytes_ = (std::max<std::int64_t>)(0, bytes);
  return *this;
}

SubscriberOptions& SubscriberOptions::set_max_concurrency(std::size_t v) {
  max_concurrency_ = v == 0 ? DefaultMaxConcurrency() : v;
  return *this;
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
