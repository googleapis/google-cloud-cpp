// Copyright 2024 Google LLC
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
// source: google/cloud/backupdr/v1/backupdr.proto

#include "google/cloud/backupdr/v1/backup_dr_connection_idempotency_policy.h"
#include <memory>

namespace google {
namespace cloud {
namespace backupdr_v1 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::Idempotency;

BackupDRConnectionIdempotencyPolicy::~BackupDRConnectionIdempotencyPolicy() =
    default;

std::unique_ptr<BackupDRConnectionIdempotencyPolicy>
BackupDRConnectionIdempotencyPolicy::clone() const {
  return std::make_unique<BackupDRConnectionIdempotencyPolicy>(*this);
}

Idempotency BackupDRConnectionIdempotencyPolicy::ListManagementServers(
    google::cloud::backupdr::v1::ListManagementServersRequest) {  // NOLINT
  return Idempotency::kIdempotent;
}

Idempotency BackupDRConnectionIdempotencyPolicy::GetManagementServer(
    google::cloud::backupdr::v1::GetManagementServerRequest const&) {
  return Idempotency::kIdempotent;
}

Idempotency BackupDRConnectionIdempotencyPolicy::CreateManagementServer(
    google::cloud::backupdr::v1::CreateManagementServerRequest const&) {
  return Idempotency::kNonIdempotent;
}

Idempotency BackupDRConnectionIdempotencyPolicy::DeleteManagementServer(
    google::cloud::backupdr::v1::DeleteManagementServerRequest const&) {
  return Idempotency::kNonIdempotent;
}

Idempotency BackupDRConnectionIdempotencyPolicy::ListLocations(
    google::cloud::location::ListLocationsRequest) {  // NOLINT
  return Idempotency::kIdempotent;
}

Idempotency BackupDRConnectionIdempotencyPolicy::GetLocation(
    google::cloud::location::GetLocationRequest const&) {
  return Idempotency::kIdempotent;
}

Idempotency BackupDRConnectionIdempotencyPolicy::SetIamPolicy(
    google::iam::v1::SetIamPolicyRequest const& request) {
  return request.policy().etag().empty() ? Idempotency::kNonIdempotent
                                         : Idempotency::kIdempotent;
}

Idempotency BackupDRConnectionIdempotencyPolicy::GetIamPolicy(
    google::iam::v1::GetIamPolicyRequest const&) {
  return Idempotency::kIdempotent;
}

Idempotency BackupDRConnectionIdempotencyPolicy::TestIamPermissions(
    google::iam::v1::TestIamPermissionsRequest const&) {
  return Idempotency::kIdempotent;
}

Idempotency BackupDRConnectionIdempotencyPolicy::ListOperations(
    google::longrunning::ListOperationsRequest) {  // NOLINT
  return Idempotency::kIdempotent;
}

Idempotency BackupDRConnectionIdempotencyPolicy::GetOperation(
    google::longrunning::GetOperationRequest const&) {
  return Idempotency::kIdempotent;
}

Idempotency BackupDRConnectionIdempotencyPolicy::DeleteOperation(
    google::longrunning::DeleteOperationRequest const&) {
  return Idempotency::kNonIdempotent;
}

Idempotency BackupDRConnectionIdempotencyPolicy::CancelOperation(
    google::longrunning::CancelOperationRequest const&) {
  return Idempotency::kNonIdempotent;
}

std::unique_ptr<BackupDRConnectionIdempotencyPolicy>
MakeDefaultBackupDRConnectionIdempotencyPolicy() {
  return std::make_unique<BackupDRConnectionIdempotencyPolicy>();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace backupdr_v1
}  // namespace cloud
}  // namespace google
