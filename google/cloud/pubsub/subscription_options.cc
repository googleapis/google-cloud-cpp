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

#include "google/cloud/pubsub/subscription_options.h"
#include <algorithm>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

SubscriptionOptions& SubscriptionOptions::set_message_count_watermarks(
    std::size_t lwm, std::size_t hwm) {
  message_count_hwm_ = (std::max<std::size_t>)(1, hwm);
  message_count_lwm_ = (std::min)(message_count_hwm_, lwm);
  return *this;
}

SubscriptionOptions& SubscriptionOptions::set_message_size_watermarks(
    std::size_t lwm, std::size_t hwm) {
  message_size_hwm_ = (std::max<std::size_t>)(1, hwm);
  message_size_lwm_ = (std::min)(message_size_hwm_, lwm);
  return *this;
}

SubscriptionOptions& SubscriptionOptions::set_concurrency_watermarks(
    std::size_t lwm, std::size_t hwm) {
  concurrency_hwm_ = (std::max<std::size_t>)(1, hwm);
  concurrency_lwm_ = (std::min)(concurrency_hwm_, lwm);
  return *this;
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
