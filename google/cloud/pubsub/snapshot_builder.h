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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SNAPSHOT_BUILDER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SNAPSHOT_BUILDER_H

#include "google/cloud/pubsub/snapshot.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/internal/time_utils.h"
#include <google/protobuf/util/field_mask_util.h>
#include <google/pubsub/v1/pubsub.pb.h>
#include <set>
#include <string>

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Build a request to create a Cloud Pub/Sub snapshot.
 *
 * Makes it easier to create the protobuf messages consumed by
 * `SubscriptionAdminClient`.  The main advantages are:
 *
 * - Use a fluent API to set multiple values when constructing complex objects.
 * - Automatically compute the set of paths for update requests.
 */
class SnapshotBuilder {
 public:
  SnapshotBuilder() = default;

  /// Build a protocol buffer message to create snapshots with server-assigned
  /// ids.
  google::pubsub::v1::CreateSnapshotRequest BuildCreateRequest(
      Subscription const& subscription) &&;

  /// Build a protocol buffer message to create snapshots with
  /// application-assigned ids.
  google::pubsub::v1::CreateSnapshotRequest BuildCreateRequest(
      Subscription const& subscription, Snapshot const& snapshot) &&;

  /// Build a protocol buffer message to update an existing snapshot.
  google::pubsub::v1::UpdateSnapshotRequest BuildUpdateRequest(
      Snapshot const& snapshot) &&;

  /// @name Setters for each protocol buffer field.
  ///@{
  SnapshotBuilder& add_label(std::string const& key,
                             std::string const& value) & {
    using value_type =
        google::protobuf::Map<std::string, std::string>::value_type;
    proto_.mutable_labels()->insert(value_type(key, value));
    paths_.insert("labels");
    return *this;
  }
  SnapshotBuilder&& add_label(std::string const& key,
                              std::string const& value) && {
    return std::move(add_label(key, value));
  }

  SnapshotBuilder& clear_labels() & {
    proto_.clear_labels();
    paths_.insert("labels");
    return *this;
  }
  SnapshotBuilder&& clear_labels() && { return std::move(clear_labels()); }
  ///@}

 private:
  google::pubsub::v1::Snapshot proto_;
  std::set<std::string> paths_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SNAPSHOT_BUILDER_H
