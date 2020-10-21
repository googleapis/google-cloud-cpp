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

#include "google/cloud/pubsub/internal/ordering_key_publisher_connection.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

future<StatusOr<std::string>> OrderingKeyPublisherConnection::Publish(
    pubsub::Message m) {
  auto child = GetChild(m.ordering_key());
  return child->Publish(std::move(m));
}

void OrderingKeyPublisherConnection::Flush() {
  // Make a copy so we can iterate without holding a lock, that is important as
  // other threads may be interested in publishing events and/or adding new
  // ordering keys. Locking while performing many (potentially long) requests is
  // just not a good idea.
  auto copy_children = [this] {
    std::lock_guard<std::mutex> lk(mu_);
    return children_;
  };
  for (auto const& kv : copy_children()) kv.second->Flush();
}

void OrderingKeyPublisherConnection::ResumePublish(
    std::string const& ordering_key) {
  auto child = GetChild(ordering_key);
  child->ResumePublish(ordering_key);
}

std::shared_ptr<MessageBatcher> OrderingKeyPublisherConnection::GetChild(
    std::string const& ordering_key) {
  std::lock_guard<std::mutex> lk(mu_);
  auto i = children_.emplace(ordering_key, std::shared_ptr<MessageBatcher>{});
  if (i.second) i.first->second = factory_(ordering_key);
  return i.first->second;
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
