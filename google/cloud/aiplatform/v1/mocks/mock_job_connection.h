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
// source: google/cloud/aiplatform/v1/job_service.proto

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_AIPLATFORM_V1_MOCKS_MOCK_JOB_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_AIPLATFORM_V1_MOCKS_MOCK_JOB_CONNECTION_H

#include "google/cloud/aiplatform/v1/job_connection.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace aiplatform_v1_mocks {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A class to mock `JobServiceConnection`.
 *
 * Application developers may want to test their code with simulated responses,
 * including errors, from an object of type `JobServiceClient`. To do so,
 * construct an object of type `JobServiceClient` with an instance of this
 * class. Then use the Google Test framework functions to program the behavior
 * of this mock.
 *
 * @see [This example][bq-mock] for how to test your application with GoogleTest.
 * While the example showcases types from the BigQuery library, the underlying
 * principles apply for any pair of `*Client` and `*Connection`.
 *
 * [bq-mock]: @cloud_cpp_docs_link{bigquery,bigquery-read-mock}
 */
class MockJobServiceConnection : public aiplatform_v1::JobServiceConnection {
 public:
  MOCK_METHOD(Options, options, (), (override));

  MOCK_METHOD(
      StatusOr<google::cloud::aiplatform::v1::CustomJob>, CreateCustomJob,
      (google::cloud::aiplatform::v1::CreateCustomJobRequest const& request),
      (override));

  MOCK_METHOD(
      StatusOr<google::cloud::aiplatform::v1::CustomJob>, GetCustomJob,
      (google::cloud::aiplatform::v1::GetCustomJobRequest const& request),
      (override));

  MOCK_METHOD((StreamRange<google::cloud::aiplatform::v1::CustomJob>),
              ListCustomJobs,
              (google::cloud::aiplatform::v1::ListCustomJobsRequest request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// DeleteCustomJob(Matcher<google::cloud::aiplatform::v1::DeleteCustomJobRequest
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(
      future<StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>,
      DeleteCustomJob,
      (google::cloud::aiplatform::v1::DeleteCustomJobRequest const& request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// EXPECT_CALL(*mock, DeleteCustomJob(_, _))
  /// @endcode
  MOCK_METHOD(
      StatusOr<google::longrunning::Operation>, DeleteCustomJob,
      (NoAwaitTag,
       google::cloud::aiplatform::v1::DeleteCustomJobRequest const& request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock, DeleteCustomJob(Matcher<google::longrunning::Operation
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(
      future<StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>,
      DeleteCustomJob, (google::longrunning::Operation const& operation),
      (override));

  MOCK_METHOD(
      Status, CancelCustomJob,
      (google::cloud::aiplatform::v1::CancelCustomJobRequest const& request),
      (override));

  MOCK_METHOD(
      StatusOr<google::cloud::aiplatform::v1::DataLabelingJob>,
      CreateDataLabelingJob,
      (google::cloud::aiplatform::v1::CreateDataLabelingJobRequest const&
           request),
      (override));

  MOCK_METHOD(
      StatusOr<google::cloud::aiplatform::v1::DataLabelingJob>,
      GetDataLabelingJob,
      (google::cloud::aiplatform::v1::GetDataLabelingJobRequest const& request),
      (override));

  MOCK_METHOD(
      (StreamRange<google::cloud::aiplatform::v1::DataLabelingJob>),
      ListDataLabelingJobs,
      (google::cloud::aiplatform::v1::ListDataLabelingJobsRequest request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// DeleteDataLabelingJob(Matcher<google::cloud::aiplatform::v1::DeleteDataLabelingJobRequest
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(
      future<StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>,
      DeleteDataLabelingJob,
      (google::cloud::aiplatform::v1::DeleteDataLabelingJobRequest const&
           request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// EXPECT_CALL(*mock, DeleteDataLabelingJob(_, _))
  /// @endcode
  MOCK_METHOD(
      StatusOr<google::longrunning::Operation>, DeleteDataLabelingJob,
      (NoAwaitTag,
       google::cloud::aiplatform::v1::DeleteDataLabelingJobRequest const&
           request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// DeleteDataLabelingJob(Matcher<google::longrunning::Operation const&>(_)))
  /// @endcode
  MOCK_METHOD(
      future<StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>,
      DeleteDataLabelingJob, (google::longrunning::Operation const& operation),
      (override));

  MOCK_METHOD(
      Status, CancelDataLabelingJob,
      (google::cloud::aiplatform::v1::CancelDataLabelingJobRequest const&
           request),
      (override));

  MOCK_METHOD(StatusOr<google::cloud::aiplatform::v1::HyperparameterTuningJob>,
              CreateHyperparameterTuningJob,
              (google::cloud::aiplatform::v1::
                   CreateHyperparameterTuningJobRequest const& request),
              (override));

  MOCK_METHOD(
      StatusOr<google::cloud::aiplatform::v1::HyperparameterTuningJob>,
      GetHyperparameterTuningJob,
      (google::cloud::aiplatform::v1::GetHyperparameterTuningJobRequest const&
           request),
      (override));

  MOCK_METHOD(
      (StreamRange<google::cloud::aiplatform::v1::HyperparameterTuningJob>),
      ListHyperparameterTuningJobs,
      (google::cloud::aiplatform::v1::ListHyperparameterTuningJobsRequest
           request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// DeleteHyperparameterTuningJob(Matcher<google::cloud::aiplatform::v1::DeleteHyperparameterTuningJobRequest
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(
      future<StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>,
      DeleteHyperparameterTuningJob,
      (google::cloud::aiplatform::v1::
           DeleteHyperparameterTuningJobRequest const& request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// EXPECT_CALL(*mock, DeleteHyperparameterTuningJob(_, _))
  /// @endcode
  MOCK_METHOD(StatusOr<google::longrunning::Operation>,
              DeleteHyperparameterTuningJob,
              (NoAwaitTag,
               google::cloud::aiplatform::v1::
                   DeleteHyperparameterTuningJobRequest const& request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// DeleteHyperparameterTuningJob(Matcher<google::longrunning::Operation
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(
      future<StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>,
      DeleteHyperparameterTuningJob,
      (google::longrunning::Operation const& operation), (override));

  MOCK_METHOD(Status, CancelHyperparameterTuningJob,
              (google::cloud::aiplatform::v1::
                   CancelHyperparameterTuningJobRequest const& request),
              (override));

  MOCK_METHOD(
      StatusOr<google::cloud::aiplatform::v1::NasJob>, CreateNasJob,
      (google::cloud::aiplatform::v1::CreateNasJobRequest const& request),
      (override));

  MOCK_METHOD(StatusOr<google::cloud::aiplatform::v1::NasJob>, GetNasJob,
              (google::cloud::aiplatform::v1::GetNasJobRequest const& request),
              (override));

  MOCK_METHOD((StreamRange<google::cloud::aiplatform::v1::NasJob>), ListNasJobs,
              (google::cloud::aiplatform::v1::ListNasJobsRequest request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// DeleteNasJob(Matcher<google::cloud::aiplatform::v1::DeleteNasJobRequest
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(
      future<StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>,
      DeleteNasJob,
      (google::cloud::aiplatform::v1::DeleteNasJobRequest const& request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// EXPECT_CALL(*mock, DeleteNasJob(_, _))
  /// @endcode
  MOCK_METHOD(
      StatusOr<google::longrunning::Operation>, DeleteNasJob,
      (NoAwaitTag,
       google::cloud::aiplatform::v1::DeleteNasJobRequest const& request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock, DeleteNasJob(Matcher<google::longrunning::Operation
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(
      future<StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>,
      DeleteNasJob, (google::longrunning::Operation const& operation),
      (override));

  MOCK_METHOD(
      Status, CancelNasJob,
      (google::cloud::aiplatform::v1::CancelNasJobRequest const& request),
      (override));

  MOCK_METHOD(
      StatusOr<google::cloud::aiplatform::v1::NasTrialDetail>,
      GetNasTrialDetail,
      (google::cloud::aiplatform::v1::GetNasTrialDetailRequest const& request),
      (override));

  MOCK_METHOD(
      (StreamRange<google::cloud::aiplatform::v1::NasTrialDetail>),
      ListNasTrialDetails,
      (google::cloud::aiplatform::v1::ListNasTrialDetailsRequest request),
      (override));

  MOCK_METHOD(
      StatusOr<google::cloud::aiplatform::v1::BatchPredictionJob>,
      CreateBatchPredictionJob,
      (google::cloud::aiplatform::v1::CreateBatchPredictionJobRequest const&
           request),
      (override));

  MOCK_METHOD(
      StatusOr<google::cloud::aiplatform::v1::BatchPredictionJob>,
      GetBatchPredictionJob,
      (google::cloud::aiplatform::v1::GetBatchPredictionJobRequest const&
           request),
      (override));

  MOCK_METHOD(
      (StreamRange<google::cloud::aiplatform::v1::BatchPredictionJob>),
      ListBatchPredictionJobs,
      (google::cloud::aiplatform::v1::ListBatchPredictionJobsRequest request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// DeleteBatchPredictionJob(Matcher<google::cloud::aiplatform::v1::DeleteBatchPredictionJobRequest
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(
      future<StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>,
      DeleteBatchPredictionJob,
      (google::cloud::aiplatform::v1::DeleteBatchPredictionJobRequest const&
           request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// EXPECT_CALL(*mock, DeleteBatchPredictionJob(_, _))
  /// @endcode
  MOCK_METHOD(
      StatusOr<google::longrunning::Operation>, DeleteBatchPredictionJob,
      (NoAwaitTag,
       google::cloud::aiplatform::v1::DeleteBatchPredictionJobRequest const&
           request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// DeleteBatchPredictionJob(Matcher<google::longrunning::Operation
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(
      future<StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>,
      DeleteBatchPredictionJob,
      (google::longrunning::Operation const& operation), (override));

  MOCK_METHOD(
      Status, CancelBatchPredictionJob,
      (google::cloud::aiplatform::v1::CancelBatchPredictionJobRequest const&
           request),
      (override));

  MOCK_METHOD(
      StatusOr<google::cloud::aiplatform::v1::ModelDeploymentMonitoringJob>,
      CreateModelDeploymentMonitoringJob,
      (google::cloud::aiplatform::v1::
           CreateModelDeploymentMonitoringJobRequest const& request),
      (override));

  MOCK_METHOD(
      (StreamRange<
          google::cloud::aiplatform::v1::ModelMonitoringStatsAnomalies>),
      SearchModelDeploymentMonitoringStatsAnomalies,
      (google::cloud::aiplatform::v1::
           SearchModelDeploymentMonitoringStatsAnomaliesRequest request),
      (override));

  MOCK_METHOD(
      StatusOr<google::cloud::aiplatform::v1::ModelDeploymentMonitoringJob>,
      GetModelDeploymentMonitoringJob,
      (google::cloud::aiplatform::v1::
           GetModelDeploymentMonitoringJobRequest const& request),
      (override));

  MOCK_METHOD(
      (StreamRange<
          google::cloud::aiplatform::v1::ModelDeploymentMonitoringJob>),
      ListModelDeploymentMonitoringJobs,
      (google::cloud::aiplatform::v1::ListModelDeploymentMonitoringJobsRequest
           request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// UpdateModelDeploymentMonitoringJob(Matcher<google::cloud::aiplatform::v1::UpdateModelDeploymentMonitoringJobRequest
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<
                  google::cloud::aiplatform::v1::ModelDeploymentMonitoringJob>>,
              UpdateModelDeploymentMonitoringJob,
              (google::cloud::aiplatform::v1::
                   UpdateModelDeploymentMonitoringJobRequest const& request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// EXPECT_CALL(*mock, UpdateModelDeploymentMonitoringJob(_, _))
  /// @endcode
  MOCK_METHOD(StatusOr<google::longrunning::Operation>,
              UpdateModelDeploymentMonitoringJob,
              (NoAwaitTag,
               google::cloud::aiplatform::v1::
                   UpdateModelDeploymentMonitoringJobRequest const& request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// UpdateModelDeploymentMonitoringJob(Matcher<google::longrunning::Operation
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<
                  google::cloud::aiplatform::v1::ModelDeploymentMonitoringJob>>,
              UpdateModelDeploymentMonitoringJob,
              (google::longrunning::Operation const& operation), (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// DeleteModelDeploymentMonitoringJob(Matcher<google::cloud::aiplatform::v1::DeleteModelDeploymentMonitoringJobRequest
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(
      future<StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>,
      DeleteModelDeploymentMonitoringJob,
      (google::cloud::aiplatform::v1::
           DeleteModelDeploymentMonitoringJobRequest const& request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// EXPECT_CALL(*mock, DeleteModelDeploymentMonitoringJob(_, _))
  /// @endcode
  MOCK_METHOD(StatusOr<google::longrunning::Operation>,
              DeleteModelDeploymentMonitoringJob,
              (NoAwaitTag,
               google::cloud::aiplatform::v1::
                   DeleteModelDeploymentMonitoringJobRequest const& request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// DeleteModelDeploymentMonitoringJob(Matcher<google::longrunning::Operation
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(
      future<StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>,
      DeleteModelDeploymentMonitoringJob,
      (google::longrunning::Operation const& operation), (override));

  MOCK_METHOD(Status, PauseModelDeploymentMonitoringJob,
              (google::cloud::aiplatform::v1::
                   PauseModelDeploymentMonitoringJobRequest const& request),
              (override));

  MOCK_METHOD(Status, ResumeModelDeploymentMonitoringJob,
              (google::cloud::aiplatform::v1::
                   ResumeModelDeploymentMonitoringJobRequest const& request),
              (override));

  MOCK_METHOD((StreamRange<google::cloud::location::Location>), ListLocations,
              (google::cloud::location::ListLocationsRequest request),
              (override));

  MOCK_METHOD(StatusOr<google::cloud::location::Location>, GetLocation,
              (google::cloud::location::GetLocationRequest const& request),
              (override));

  MOCK_METHOD(StatusOr<google::iam::v1::Policy>, SetIamPolicy,
              (google::iam::v1::SetIamPolicyRequest const& request),
              (override));

  MOCK_METHOD(StatusOr<google::iam::v1::Policy>, GetIamPolicy,
              (google::iam::v1::GetIamPolicyRequest const& request),
              (override));

  MOCK_METHOD(StatusOr<google::iam::v1::TestIamPermissionsResponse>,
              TestIamPermissions,
              (google::iam::v1::TestIamPermissionsRequest const& request),
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

  MOCK_METHOD(StatusOr<google::longrunning::Operation>, WaitOperation,
              (google::longrunning::WaitOperationRequest const& request),
              (override));
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace aiplatform_v1_mocks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_AIPLATFORM_V1_MOCKS_MOCK_JOB_CONNECTION_H
