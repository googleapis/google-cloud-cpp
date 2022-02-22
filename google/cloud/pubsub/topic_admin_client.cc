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

#include "google/cloud/pubsub/topic_admin_client.h"
#include "google/cloud/pubsub/internal/defaults.h"

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

TopicAdminClient::TopicAdminClient(
    std::shared_ptr<TopicAdminConnection> connection, Options opts)
    : connection_(std::move(connection)),
      options_(internal::MergeOptions(
          std::move(opts),
          pubsub_internal::DefaultCommonOptions(connection_->options()))) {}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
