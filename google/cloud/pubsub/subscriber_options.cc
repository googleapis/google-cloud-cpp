// Copyright 2020 Google LLC
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

#include "google/cloud/pubsub/subscriber_options.h"
#include "google/cloud/pubsub/internal/defaults.h"
#include <algorithm>

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using seconds = std::chrono::seconds;

// Cannot delegate on `SubscriberOptions(Options)` as it is deprecated.
SubscriberOptions::SubscriberOptions() {
  opts_ = pubsub_internal::DefaultSubscriberOptionsOnly(Options{});
}

SubscriberOptions::SubscriberOptions(Options opts) {
  internal::CheckExpectedOptions<SubscriberOptionList>(opts, __func__);
  opts_ = pubsub_internal::DefaultSubscriberOptionsOnly(std::move(opts));
}

SubscriberOptions& SubscriberOptions::set_max_deadline_extension(
    seconds extension) {
  opts_.set<MaxDeadlineExtensionOption>(
      (std::max)((std::min)(extension, seconds(600)), seconds(10)));
  return *this;
}

SubscriberOptions& SubscriberOptions::set_max_outstanding_messages(
    std::int64_t message_count) {
  opts_.set<MaxOutstandingMessagesOption>(
      (std::max<std::int64_t>)(0, message_count));
  return *this;
}

SubscriberOptions& SubscriberOptions::set_max_outstanding_bytes(
    std::int64_t bytes) {
  opts_.set<MaxOutstandingBytesOption>((std::max<std::int64_t>)(0, bytes));
  return *this;
}

SubscriberOptions& SubscriberOptions::set_max_concurrency(std::size_t v) {
  opts_.set<MaxConcurrencyOption>(v == 0 ? pubsub_internal::DefaultThreadCount()
                                         : v);
  return *this;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
