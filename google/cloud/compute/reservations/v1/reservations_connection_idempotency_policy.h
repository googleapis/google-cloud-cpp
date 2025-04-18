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
// source: google/cloud/compute/reservations/v1/reservations.proto

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_COMPUTE_RESERVATIONS_V1_RESERVATIONS_CONNECTION_IDEMPOTENCY_POLICY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_COMPUTE_RESERVATIONS_V1_RESERVATIONS_CONNECTION_IDEMPOTENCY_POLICY_H

#include "google/cloud/idempotency.h"
#include "google/cloud/version.h"
#include <google/cloud/compute/reservations/v1/reservations.pb.h>
#include <memory>

namespace google {
namespace cloud {
namespace compute_reservations_v1 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class ReservationsConnectionIdempotencyPolicy {
 public:
  virtual ~ReservationsConnectionIdempotencyPolicy();

  /// Create a new copy of this object.
  virtual std::unique_ptr<ReservationsConnectionIdempotencyPolicy> clone()
      const;

  virtual google::cloud::Idempotency AggregatedListReservations(
      google::cloud::cpp::compute::reservations::v1::
          AggregatedListReservationsRequest request);

  virtual google::cloud::Idempotency DeleteReservation(
      google::cloud::cpp::compute::reservations::v1::
          DeleteReservationRequest const& request);

  virtual google::cloud::Idempotency GetReservation(
      google::cloud::cpp::compute::reservations::v1::
          GetReservationRequest const& request);

  virtual google::cloud::Idempotency GetIamPolicy(
      google::cloud::cpp::compute::reservations::v1::GetIamPolicyRequest const&
          request);

  virtual google::cloud::Idempotency InsertReservation(
      google::cloud::cpp::compute::reservations::v1::
          InsertReservationRequest const& request);

  virtual google::cloud::Idempotency ListReservations(
      google::cloud::cpp::compute::reservations::v1::ListReservationsRequest
          request);

  virtual google::cloud::Idempotency PerformMaintenance(
      google::cloud::cpp::compute::reservations::v1::
          PerformMaintenanceRequest const& request);

  virtual google::cloud::Idempotency Resize(
      google::cloud::cpp::compute::reservations::v1::ResizeRequest const&
          request);

  virtual google::cloud::Idempotency SetIamPolicy(
      google::cloud::cpp::compute::reservations::v1::SetIamPolicyRequest const&
          request);

  virtual google::cloud::Idempotency TestIamPermissions(
      google::cloud::cpp::compute::reservations::v1::
          TestIamPermissionsRequest const& request);

  virtual google::cloud::Idempotency UpdateReservation(
      google::cloud::cpp::compute::reservations::v1::
          UpdateReservationRequest const& request);
};

std::unique_ptr<ReservationsConnectionIdempotencyPolicy>
MakeDefaultReservationsConnectionIdempotencyPolicy();

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace compute_reservations_v1
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_COMPUTE_RESERVATIONS_V1_RESERVATIONS_CONNECTION_IDEMPOTENCY_POLICY_H
