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
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * Build a request to create a Cloud Pub/Sub snapshot.
 */
class SnapshotBuilder {
 public:
  SnapshotBuilder() = default;

  /// Build a CreateSnapshotRequest where the server assigns the snapshot id.
  google::pubsub::v1::CreateSnapshotRequest BuildCreateRequest(
      Subscription const& subscription) &&;

  /// Build a CreateSnapshotRequest where the application assigns the snapshot
  /// id.
  google::pubsub::v1::CreateSnapshotRequest BuildCreateRequest(
      Subscription const& subscription, Snapshot const& snapshot) &&;

  /// Build a UpdateSnapshotRequest.
  google::pubsub::v1::UpdateSnapshotRequest BuildUpdateRequest(
      Snapshot const& snapshot) &&;

  SnapshotBuilder& add_label(std::string const& key,
                             std::string const& value) & {
    using value_type = protobuf::Map<std::string, std::string>::value_type;
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

 private:
  google::pubsub::v1::Snapshot proto_;
  std::set<std::string> paths_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SNAPSHOT_BUILDER_H
