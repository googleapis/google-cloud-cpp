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
// source: google/cloud/kms/v1/autokey_admin.proto

#include "google/cloud/kms/v1/autokey_admin_connection.h"
#include "google/cloud/kms/v1/autokey_admin_options.h"
#include "google/cloud/kms/v1/internal/autokey_admin_connection_impl.h"
#include "google/cloud/kms/v1/internal/autokey_admin_option_defaults.h"
#include "google/cloud/kms/v1/internal/autokey_admin_stub_factory.h"
#include "google/cloud/kms/v1/internal/autokey_admin_tracing_connection.h"
#include "google/cloud/background_threads.h"
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/pagination_range.h"
#include "google/cloud/internal/unified_grpc_credentials.h"
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace kms_v1 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

AutokeyAdminConnection::~AutokeyAdminConnection() = default;

StatusOr<google::cloud::kms::v1::AutokeyConfig>
AutokeyAdminConnection::UpdateAutokeyConfig(
    google::cloud::kms::v1::UpdateAutokeyConfigRequest const&) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<google::cloud::kms::v1::AutokeyConfig>
AutokeyAdminConnection::GetAutokeyConfig(
    google::cloud::kms::v1::GetAutokeyConfigRequest const&) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<google::cloud::kms::v1::ShowEffectiveAutokeyConfigResponse>
AutokeyAdminConnection::ShowEffectiveAutokeyConfig(
    google::cloud::kms::v1::ShowEffectiveAutokeyConfigRequest const&) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StreamRange<google::cloud::location::Location>
AutokeyAdminConnection::ListLocations(
    google::cloud::location::
        ListLocationsRequest) {  // NOLINT(performance-unnecessary-value-param)
  return google::cloud::internal::MakeUnimplementedPaginationRange<
      StreamRange<google::cloud::location::Location>>();
}

StatusOr<google::cloud::location::Location> AutokeyAdminConnection::GetLocation(
    google::cloud::location::GetLocationRequest const&) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<google::iam::v1::Policy> AutokeyAdminConnection::SetIamPolicy(
    google::iam::v1::SetIamPolicyRequest const&) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<google::iam::v1::Policy> AutokeyAdminConnection::GetIamPolicy(
    google::iam::v1::GetIamPolicyRequest const&) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<google::iam::v1::TestIamPermissionsResponse>
AutokeyAdminConnection::TestIamPermissions(
    google::iam::v1::TestIamPermissionsRequest const&) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<google::longrunning::Operation> AutokeyAdminConnection::GetOperation(
    google::longrunning::GetOperationRequest const&) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

std::shared_ptr<AutokeyAdminConnection> MakeAutokeyAdminConnection(
    Options options) {
  internal::CheckExpectedOptions<CommonOptionList, GrpcOptionList,
                                 UnifiedCredentialsOptionList,
                                 AutokeyAdminPolicyOptionList>(options,
                                                               __func__);
  options = kms_v1_internal::AutokeyAdminDefaultOptions(std::move(options));
  auto background = internal::MakeBackgroundThreadsFactory(options)();
  auto auth = internal::CreateAuthenticationStrategy(background->cq(), options);
  auto stub =
      kms_v1_internal::CreateDefaultAutokeyAdminStub(std::move(auth), options);
  return kms_v1_internal::MakeAutokeyAdminTracingConnection(
      std::make_shared<kms_v1_internal::AutokeyAdminConnectionImpl>(
          std::move(background), std::move(stub), std::move(options)));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace kms_v1
}  // namespace cloud
}  // namespace google
