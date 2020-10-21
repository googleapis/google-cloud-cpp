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

#include "google/cloud/pubsub/internal/rejects_with_ordering_key.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

future<StatusOr<std::string>> RejectsWithOrderingKey::Publish(
    pubsub::Message m) {
  if (!m.ordering_key().empty()) {
    return google::cloud::make_ready_future(StatusOr<std::string>(
        Status(StatusCode::kInvalidArgument,
               "Attempted to publish a message with an ordering"
               " key with a publisher that does not have message"
               " ordering enabled.")));
  }
  return child_->Publish(std::move(m));
}

void RejectsWithOrderingKey::Flush() { return child_->Flush(); }

void RejectsWithOrderingKey::ResumePublish(std::string const&) {
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
