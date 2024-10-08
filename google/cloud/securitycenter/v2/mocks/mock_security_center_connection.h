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
// source: google/cloud/securitycenter/v2/securitycenter_service.proto

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SECURITYCENTER_V2_MOCKS_MOCK_SECURITY_CENTER_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SECURITYCENTER_V2_MOCKS_MOCK_SECURITY_CENTER_CONNECTION_H

#include "google/cloud/securitycenter/v2/security_center_connection.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace securitycenter_v2_mocks {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A class to mock `SecurityCenterConnection`.
 *
 * Application developers may want to test their code with simulated responses,
 * including errors, from an object of type `SecurityCenterClient`. To do so,
 * construct an object of type `SecurityCenterClient` with an instance of this
 * class. Then use the Google Test framework functions to program the behavior
 * of this mock.
 *
 * @see [This example][bq-mock] for how to test your application with GoogleTest.
 * While the example showcases types from the BigQuery library, the underlying
 * principles apply for any pair of `*Client` and `*Connection`.
 *
 * [bq-mock]: @cloud_cpp_docs_link{bigquery,bigquery-read-mock}
 */
class MockSecurityCenterConnection
    : public securitycenter_v2::SecurityCenterConnection {
 public:
  MOCK_METHOD(Options, options, (), (override));

  MOCK_METHOD(StatusOr<google::cloud::securitycenter::v2::
                           BatchCreateResourceValueConfigsResponse>,
              BatchCreateResourceValueConfigs,
              (google::cloud::securitycenter::v2::
                   BatchCreateResourceValueConfigsRequest const& request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// BulkMuteFindings(Matcher<google::cloud::securitycenter::v2::BulkMuteFindingsRequest
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<
                  google::cloud::securitycenter::v2::BulkMuteFindingsResponse>>,
              BulkMuteFindings,
              (google::cloud::securitycenter::v2::BulkMuteFindingsRequest const&
                   request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// EXPECT_CALL(*mock, BulkMuteFindings(_, _))
  /// @endcode
  MOCK_METHOD(StatusOr<google::longrunning::Operation>, BulkMuteFindings,
              (NoAwaitTag,
               google::cloud::securitycenter::v2::BulkMuteFindingsRequest const&
                   request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock, BulkMuteFindings(Matcher<google::longrunning::Operation
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<
                  google::cloud::securitycenter::v2::BulkMuteFindingsResponse>>,
              BulkMuteFindings,
              (google::longrunning::Operation const& operation), (override));

  MOCK_METHOD(
      StatusOr<google::cloud::securitycenter::v2::BigQueryExport>,
      CreateBigQueryExport,
      (google::cloud::securitycenter::v2::CreateBigQueryExportRequest const&
           request),
      (override));

  MOCK_METHOD(
      StatusOr<google::cloud::securitycenter::v2::Finding>, CreateFinding,
      (google::cloud::securitycenter::v2::CreateFindingRequest const& request),
      (override));

  MOCK_METHOD(StatusOr<google::cloud::securitycenter::v2::MuteConfig>,
              CreateMuteConfig,
              (google::cloud::securitycenter::v2::CreateMuteConfigRequest const&
                   request),
              (override));

  MOCK_METHOD(
      StatusOr<google::cloud::securitycenter::v2::NotificationConfig>,
      CreateNotificationConfig,
      (google::cloud::securitycenter::v2::CreateNotificationConfigRequest const&
           request),
      (override));

  MOCK_METHOD(
      StatusOr<google::cloud::securitycenter::v2::Source>, CreateSource,
      (google::cloud::securitycenter::v2::CreateSourceRequest const& request),
      (override));

  MOCK_METHOD(
      Status, DeleteBigQueryExport,
      (google::cloud::securitycenter::v2::DeleteBigQueryExportRequest const&
           request),
      (override));

  MOCK_METHOD(Status, DeleteMuteConfig,
              (google::cloud::securitycenter::v2::DeleteMuteConfigRequest const&
                   request),
              (override));

  MOCK_METHOD(
      Status, DeleteNotificationConfig,
      (google::cloud::securitycenter::v2::DeleteNotificationConfigRequest const&
           request),
      (override));

  MOCK_METHOD(Status, DeleteResourceValueConfig,
              (google::cloud::securitycenter::v2::
                   DeleteResourceValueConfigRequest const& request),
              (override));

  MOCK_METHOD(
      StatusOr<google::cloud::securitycenter::v2::BigQueryExport>,
      GetBigQueryExport,
      (google::cloud::securitycenter::v2::GetBigQueryExportRequest const&
           request),
      (override));

  MOCK_METHOD(
      StatusOr<google::cloud::securitycenter::v2::Simulation>, GetSimulation,
      (google::cloud::securitycenter::v2::GetSimulationRequest const& request),
      (override));

  MOCK_METHOD(
      StatusOr<google::cloud::securitycenter::v2::ValuedResource>,
      GetValuedResource,
      (google::cloud::securitycenter::v2::GetValuedResourceRequest const&
           request),
      (override));

  MOCK_METHOD(StatusOr<google::iam::v1::Policy>, GetIamPolicy,
              (google::iam::v1::GetIamPolicyRequest const& request),
              (override));

  MOCK_METHOD(
      StatusOr<google::cloud::securitycenter::v2::MuteConfig>, GetMuteConfig,
      (google::cloud::securitycenter::v2::GetMuteConfigRequest const& request),
      (override));

  MOCK_METHOD(
      StatusOr<google::cloud::securitycenter::v2::NotificationConfig>,
      GetNotificationConfig,
      (google::cloud::securitycenter::v2::GetNotificationConfigRequest const&
           request),
      (override));

  MOCK_METHOD(
      StatusOr<google::cloud::securitycenter::v2::ResourceValueConfig>,
      GetResourceValueConfig,
      (google::cloud::securitycenter::v2::GetResourceValueConfigRequest const&
           request),
      (override));

  MOCK_METHOD(
      StatusOr<google::cloud::securitycenter::v2::Source>, GetSource,
      (google::cloud::securitycenter::v2::GetSourceRequest const& request),
      (override));

  MOCK_METHOD((StreamRange<google::cloud::securitycenter::v2::GroupResult>),
              GroupFindings,
              (google::cloud::securitycenter::v2::GroupFindingsRequest request),
              (override));

  MOCK_METHOD(
      (StreamRange<google::cloud::securitycenter::v2::AttackPath>),
      ListAttackPaths,
      (google::cloud::securitycenter::v2::ListAttackPathsRequest request),
      (override));

  MOCK_METHOD(
      (StreamRange<google::cloud::securitycenter::v2::BigQueryExport>),
      ListBigQueryExports,
      (google::cloud::securitycenter::v2::ListBigQueryExportsRequest request),
      (override));

  MOCK_METHOD((StreamRange<google::cloud::securitycenter::v2::
                               ListFindingsResponse::ListFindingsResult>),
              ListFindings,
              (google::cloud::securitycenter::v2::ListFindingsRequest request),
              (override));

  MOCK_METHOD(
      (StreamRange<google::cloud::securitycenter::v2::MuteConfig>),
      ListMuteConfigs,
      (google::cloud::securitycenter::v2::ListMuteConfigsRequest request),
      (override));

  MOCK_METHOD(
      (StreamRange<google::cloud::securitycenter::v2::NotificationConfig>),
      ListNotificationConfigs,
      (google::cloud::securitycenter::v2::ListNotificationConfigsRequest
           request),
      (override));

  MOCK_METHOD(
      (StreamRange<google::cloud::securitycenter::v2::ResourceValueConfig>),
      ListResourceValueConfigs,
      (google::cloud::securitycenter::v2::ListResourceValueConfigsRequest
           request),
      (override));

  MOCK_METHOD((StreamRange<google::cloud::securitycenter::v2::Source>),
              ListSources,
              (google::cloud::securitycenter::v2::ListSourcesRequest request),
              (override));

  MOCK_METHOD(
      (StreamRange<google::cloud::securitycenter::v2::ValuedResource>),
      ListValuedResources,
      (google::cloud::securitycenter::v2::ListValuedResourcesRequest request),
      (override));

  MOCK_METHOD(StatusOr<google::cloud::securitycenter::v2::Finding>,
              SetFindingState,
              (google::cloud::securitycenter::v2::SetFindingStateRequest const&
                   request),
              (override));

  MOCK_METHOD(StatusOr<google::iam::v1::Policy>, SetIamPolicy,
              (google::iam::v1::SetIamPolicyRequest const& request),
              (override));

  MOCK_METHOD(
      StatusOr<google::cloud::securitycenter::v2::Finding>, SetMute,
      (google::cloud::securitycenter::v2::SetMuteRequest const& request),
      (override));

  MOCK_METHOD(StatusOr<google::iam::v1::TestIamPermissionsResponse>,
              TestIamPermissions,
              (google::iam::v1::TestIamPermissionsRequest const& request),
              (override));

  MOCK_METHOD(
      StatusOr<google::cloud::securitycenter::v2::BigQueryExport>,
      UpdateBigQueryExport,
      (google::cloud::securitycenter::v2::UpdateBigQueryExportRequest const&
           request),
      (override));

  MOCK_METHOD(
      StatusOr<google::cloud::securitycenter::v2::ExternalSystem>,
      UpdateExternalSystem,
      (google::cloud::securitycenter::v2::UpdateExternalSystemRequest const&
           request),
      (override));

  MOCK_METHOD(
      StatusOr<google::cloud::securitycenter::v2::Finding>, UpdateFinding,
      (google::cloud::securitycenter::v2::UpdateFindingRequest const& request),
      (override));

  MOCK_METHOD(StatusOr<google::cloud::securitycenter::v2::MuteConfig>,
              UpdateMuteConfig,
              (google::cloud::securitycenter::v2::UpdateMuteConfigRequest const&
                   request),
              (override));

  MOCK_METHOD(
      StatusOr<google::cloud::securitycenter::v2::NotificationConfig>,
      UpdateNotificationConfig,
      (google::cloud::securitycenter::v2::UpdateNotificationConfigRequest const&
           request),
      (override));

  MOCK_METHOD(StatusOr<google::cloud::securitycenter::v2::ResourceValueConfig>,
              UpdateResourceValueConfig,
              (google::cloud::securitycenter::v2::
                   UpdateResourceValueConfigRequest const& request),
              (override));

  MOCK_METHOD(
      StatusOr<google::cloud::securitycenter::v2::SecurityMarks>,
      UpdateSecurityMarks,
      (google::cloud::securitycenter::v2::UpdateSecurityMarksRequest const&
           request),
      (override));

  MOCK_METHOD(
      StatusOr<google::cloud::securitycenter::v2::Source>, UpdateSource,
      (google::cloud::securitycenter::v2::UpdateSourceRequest const& request),
      (override));

  MOCK_METHOD((StreamRange<google::longrunning::Operation>), ListOperations,
              (google::longrunning::ListOperationsRequest request), (override));

  MOCK_METHOD(StatusOr<google::longrunning::Operation>, GetOperation,
              (google::longrunning::GetOperationRequest const& request),
              (override));

  MOCK_METHOD(Status, DeleteOperation,
              (google::longrunning::DeleteOperationRequest const& request),
              (override));

  MOCK_METHOD(Status, CancelOperation,
              (google::longrunning::CancelOperationRequest const& request),
              (override));
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace securitycenter_v2_mocks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SECURITYCENTER_V2_MOCKS_MOCK_SECURITY_CENTER_CONNECTION_H
