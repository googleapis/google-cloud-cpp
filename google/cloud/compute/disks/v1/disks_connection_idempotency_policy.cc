// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Generated by the Codegen C++ plugin.
// If you make any local changes, they will be lost.
// source: google/cloud/compute/disks/v1/disks.proto

#include "google/cloud/compute/disks/v1/disks_connection_idempotency_policy.h"
#include <memory>

namespace google {
namespace cloud {
namespace compute_disks_v1 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::Idempotency;

DisksConnectionIdempotencyPolicy::~DisksConnectionIdempotencyPolicy() = default;

std::unique_ptr<DisksConnectionIdempotencyPolicy>
DisksConnectionIdempotencyPolicy::clone() const {
  return std::make_unique<DisksConnectionIdempotencyPolicy>(*this);
}

Idempotency DisksConnectionIdempotencyPolicy::AddResourcePolicies(
    google::cloud::cpp::compute::disks::v1::AddResourcePoliciesRequest const&) {
  return Idempotency::kNonIdempotent;
}

Idempotency DisksConnectionIdempotencyPolicy::AggregatedListDisks(
    google::cloud::cpp::compute::disks::v1::
        AggregatedListDisksRequest) {  // NOLINT
  return Idempotency::kIdempotent;
}

Idempotency DisksConnectionIdempotencyPolicy::BulkInsert(
    google::cloud::cpp::compute::disks::v1::BulkInsertRequest const&) {
  return Idempotency::kNonIdempotent;
}

Idempotency DisksConnectionIdempotencyPolicy::BulkSetLabels(
    google::cloud::cpp::compute::disks::v1::BulkSetLabelsRequest const&) {
  return Idempotency::kNonIdempotent;
}

Idempotency DisksConnectionIdempotencyPolicy::CreateSnapshot(
    google::cloud::cpp::compute::disks::v1::CreateSnapshotRequest const&) {
  return Idempotency::kNonIdempotent;
}

Idempotency DisksConnectionIdempotencyPolicy::DeleteDisk(
    google::cloud::cpp::compute::disks::v1::DeleteDiskRequest const&) {
  return Idempotency::kNonIdempotent;
}

Idempotency DisksConnectionIdempotencyPolicy::GetDisk(
    google::cloud::cpp::compute::disks::v1::GetDiskRequest const&) {
  return Idempotency::kIdempotent;
}

Idempotency DisksConnectionIdempotencyPolicy::GetIamPolicy(
    google::cloud::cpp::compute::disks::v1::GetIamPolicyRequest const&) {
  return Idempotency::kIdempotent;
}

Idempotency DisksConnectionIdempotencyPolicy::InsertDisk(
    google::cloud::cpp::compute::disks::v1::InsertDiskRequest const&) {
  return Idempotency::kNonIdempotent;
}

Idempotency DisksConnectionIdempotencyPolicy::ListDisks(
    google::cloud::cpp::compute::disks::v1::ListDisksRequest) {  // NOLINT
  return Idempotency::kIdempotent;
}

Idempotency DisksConnectionIdempotencyPolicy::RemoveResourcePolicies(
    google::cloud::cpp::compute::disks::v1::
        RemoveResourcePoliciesRequest const&) {
  return Idempotency::kNonIdempotent;
}

Idempotency DisksConnectionIdempotencyPolicy::Resize(
    google::cloud::cpp::compute::disks::v1::ResizeRequest const&) {
  return Idempotency::kNonIdempotent;
}

Idempotency DisksConnectionIdempotencyPolicy::SetIamPolicy(
    google::cloud::cpp::compute::disks::v1::SetIamPolicyRequest const&) {
  return Idempotency::kNonIdempotent;
}

Idempotency DisksConnectionIdempotencyPolicy::SetLabels(
    google::cloud::cpp::compute::disks::v1::SetLabelsRequest const&) {
  return Idempotency::kNonIdempotent;
}

Idempotency DisksConnectionIdempotencyPolicy::StartAsyncReplication(
    google::cloud::cpp::compute::disks::v1::
        StartAsyncReplicationRequest const&) {
  return Idempotency::kNonIdempotent;
}

Idempotency DisksConnectionIdempotencyPolicy::StopAsyncReplication(
    google::cloud::cpp::compute::disks::v1::
        StopAsyncReplicationRequest const&) {
  return Idempotency::kNonIdempotent;
}

Idempotency DisksConnectionIdempotencyPolicy::StopGroupAsyncReplication(
    google::cloud::cpp::compute::disks::v1::
        StopGroupAsyncReplicationRequest const&) {
  return Idempotency::kNonIdempotent;
}

Idempotency DisksConnectionIdempotencyPolicy::TestIamPermissions(
    google::cloud::cpp::compute::disks::v1::TestIamPermissionsRequest const&) {
  return Idempotency::kNonIdempotent;
}

Idempotency DisksConnectionIdempotencyPolicy::UpdateDisk(
    google::cloud::cpp::compute::disks::v1::UpdateDiskRequest const&) {
  return Idempotency::kNonIdempotent;
}

std::unique_ptr<DisksConnectionIdempotencyPolicy>
MakeDefaultDisksConnectionIdempotencyPolicy() {
  return std::make_unique<DisksConnectionIdempotencyPolicy>();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace compute_disks_v1
}  // namespace cloud
}  // namespace google
