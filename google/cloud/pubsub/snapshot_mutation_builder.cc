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

#include "google/cloud/pubsub/snapshot_mutation_builder.h"
#include <google/protobuf/util/field_mask_util.h>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

google::pubsub::v1::CreateSnapshotRequest
SnapshotMutationBuilder::BuildCreateMutation(
    Subscription const& subscription) && {
  google::pubsub::v1::CreateSnapshotRequest request;
  request.set_subscription(subscription.FullName());
  request.mutable_labels()->swap(*proto_.mutable_labels());
  return request;
}

google::pubsub::v1::CreateSnapshotRequest
SnapshotMutationBuilder::BuildCreateMutation(Subscription const& subscription,
                                             Snapshot const& snapshot) && {
  google::pubsub::v1::CreateSnapshotRequest request;
  request.set_subscription(subscription.FullName());
  request.set_name(snapshot.FullName());
  request.mutable_labels()->swap(*proto_.mutable_labels());
  return request;
}

google::pubsub::v1::UpdateSnapshotRequest
SnapshotMutationBuilder::BuildUpdateMutation(Snapshot const& snapshot) && {
  google::pubsub::v1::UpdateSnapshotRequest request;
  for (auto const& p : paths_) {
    google::protobuf::util::FieldMaskUtil::AddPathToFieldMask<
        google::pubsub::v1::Snapshot>(p, request.mutable_update_mask());
  }
  proto_.set_name(snapshot.FullName());
  *request.mutable_snapshot() = std::move(proto_);
  return request;
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
