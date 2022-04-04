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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_ROUTING_POLICY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_ROUTING_POLICY_H

#include "google/cloud/version.h"

namespace google {
namespace cloud {
namespace pubsublite_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A partition should be an integer in the range [0, UINT32_MAX).
 */
using Partition = std::uint32_t;

/**
 * Interface for a Pub/Sub Lite routing policy that determines the partition
 * that a message should be sent to, either depending on the message's key or
 * not.
 */
class RoutingPolicy {
 public:
  virtual ~RoutingPolicy() = default;
  virtual Partition Route(Partition num_partitions) = 0;
  virtual Partition Route(std::string const& message_key,
                          Partition num_partitions) = 0;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_ROUTING_POLICY_H
