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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_AIPLATFORM_V1_JOB_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_AIPLATFORM_V1_JOB_CONNECTION_H

#include "google/cloud/aiplatform/v1/internal/job_retry_traits.h"
#include "google/cloud/aiplatform/v1/job_connection_idempotency_policy.h"
#include "google/cloud/backoff_policy.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/retry_policy_impl.h"
#include "google/cloud/no_await_tag.h"
#include "google/cloud/options.h"
#include "google/cloud/polling_policy.h"
#include "google/cloud/status_or.h"
#include "google/cloud/stream_range.h"
#include "google/cloud/version.h"
#include <google/cloud/aiplatform/v1/job_service.pb.h>
#include <google/longrunning/operations.grpc.pb.h>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace aiplatform_v1 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// The retry policy for `JobServiceConnection`.
class JobServiceRetryPolicy : public ::google::cloud::RetryPolicy {
 public:
  /// Creates a new instance of the policy, reset to the initial state.
  virtual std::unique_ptr<JobServiceRetryPolicy> clone() const = 0;
};

/**
 * A retry policy for `JobServiceConnection` based on counting errors.
 *
 * This policy stops retrying if:
 * - An RPC returns a non-transient error.
 * - More than a prescribed number of transient failures is detected.
 *
 * In this class the following status codes are treated as transient errors:
 * - [`kUnavailable`](@ref google::cloud::StatusCode)
 */
class JobServiceLimitedErrorCountRetryPolicy : public JobServiceRetryPolicy {
 public:
  /**
   * Create an instance that tolerates up to @p maximum_failures transient
   * errors.
   *
   * @note Disable the retry loop by providing an instance of this policy with
   *     @p maximum_failures == 0.
   */
  explicit JobServiceLimitedErrorCountRetryPolicy(int maximum_failures)
      : impl_(maximum_failures) {}

  JobServiceLimitedErrorCountRetryPolicy(
      JobServiceLimitedErrorCountRetryPolicy&& rhs) noexcept
      : JobServiceLimitedErrorCountRetryPolicy(rhs.maximum_failures()) {}
  JobServiceLimitedErrorCountRetryPolicy(
      JobServiceLimitedErrorCountRetryPolicy const& rhs) noexcept
      : JobServiceLimitedErrorCountRetryPolicy(rhs.maximum_failures()) {}

  int maximum_failures() const { return impl_.maximum_failures(); }

  bool OnFailure(Status const& status) override {
    return impl_.OnFailure(status);
  }
  bool IsExhausted() const override { return impl_.IsExhausted(); }
  bool IsPermanentFailure(Status const& status) const override {
    return impl_.IsPermanentFailure(status);
  }
  std::unique_ptr<JobServiceRetryPolicy> clone() const override {
    return std::make_unique<JobServiceLimitedErrorCountRetryPolicy>(
        maximum_failures());
  }

  // This is provided only for backwards compatibility.
  using BaseType = JobServiceRetryPolicy;

 private:
  google::cloud::internal::LimitedErrorCountRetryPolicy<
      aiplatform_v1_internal::JobServiceRetryTraits>
      impl_;
};

/**
 * A retry policy for `JobServiceConnection` based on elapsed time.
 *
 * This policy stops retrying if:
 * - An RPC returns a non-transient error.
 * - The elapsed time in the retry loop exceeds a prescribed duration.
 *
 * In this class the following status codes are treated as transient errors:
 * - [`kUnavailable`](@ref google::cloud::StatusCode)
 */
class JobServiceLimitedTimeRetryPolicy : public JobServiceRetryPolicy {
 public:
  /**
   * Constructor given a `std::chrono::duration<>` object.
   *
   * @tparam DurationRep a placeholder to match the `Rep` tparam for @p
   *     duration's type. The semantics of this template parameter are
   *     documented in `std::chrono::duration<>`. In brief, the underlying
   *     arithmetic type used to store the number of ticks. For our purposes it
   *     is simply a formal parameter.
   * @tparam DurationPeriod a placeholder to match the `Period` tparam for @p
   *     duration's type. The semantics of this template parameter are
   *     documented in `std::chrono::duration<>`. In brief, the length of the
   *     tick in seconds, expressed as a `std::ratio<>`. For our purposes it is
   *     simply a formal parameter.
   * @param maximum_duration the maximum time allowed before the policy expires.
   *     While the application can express this time in any units they desire,
   *     the class truncates to milliseconds.
   *
   * @see https://en.cppreference.com/w/cpp/chrono/duration for more information
   *     about `std::chrono::duration`.
   */
  template <typename DurationRep, typename DurationPeriod>
  explicit JobServiceLimitedTimeRetryPolicy(
      std::chrono::duration<DurationRep, DurationPeriod> maximum_duration)
      : impl_(maximum_duration) {}

  JobServiceLimitedTimeRetryPolicy(
      JobServiceLimitedTimeRetryPolicy&& rhs) noexcept
      : JobServiceLimitedTimeRetryPolicy(rhs.maximum_duration()) {}
  JobServiceLimitedTimeRetryPolicy(
      JobServiceLimitedTimeRetryPolicy const& rhs) noexcept
      : JobServiceLimitedTimeRetryPolicy(rhs.maximum_duration()) {}

  std::chrono::milliseconds maximum_duration() const {
    return impl_.maximum_duration();
  }

  bool OnFailure(Status const& status) override {
    return impl_.OnFailure(status);
  }
  bool IsExhausted() const override { return impl_.IsExhausted(); }
  bool IsPermanentFailure(Status const& status) const override {
    return impl_.IsPermanentFailure(status);
  }
  std::unique_ptr<JobServiceRetryPolicy> clone() const override {
    return std::make_unique<JobServiceLimitedTimeRetryPolicy>(
        maximum_duration());
  }

  // This is provided only for backwards compatibility.
  using BaseType = JobServiceRetryPolicy;

 private:
  google::cloud::internal::LimitedTimeRetryPolicy<
      aiplatform_v1_internal::JobServiceRetryTraits>
      impl_;
};

/**
 * The `JobServiceConnection` object for `JobServiceClient`.
 *
 * This interface defines virtual methods for each of the user-facing overload
 * sets in `JobServiceClient`. This allows users to inject custom behavior
 * (e.g., with a Google Mock object) when writing tests that use objects of type
 * `JobServiceClient`.
 *
 * To create a concrete instance, see `MakeJobServiceConnection()`.
 *
 * For mocking, see `aiplatform_v1_mocks::MockJobServiceConnection`.
 */
class JobServiceConnection {
 public:
  virtual ~JobServiceConnection() = 0;

  virtual Options options() { return Options{}; }

  virtual StatusOr<google::cloud::aiplatform::v1::CustomJob> CreateCustomJob(
      google::cloud::aiplatform::v1::CreateCustomJobRequest const& request);

  virtual StatusOr<google::cloud::aiplatform::v1::CustomJob> GetCustomJob(
      google::cloud::aiplatform::v1::GetCustomJobRequest const& request);

  virtual StreamRange<google::cloud::aiplatform::v1::CustomJob> ListCustomJobs(
      google::cloud::aiplatform::v1::ListCustomJobsRequest request);

  virtual future<
      StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>
  DeleteCustomJob(
      google::cloud::aiplatform::v1::DeleteCustomJobRequest const& request);

  virtual StatusOr<google::longrunning::Operation> DeleteCustomJob(
      NoAwaitTag,
      google::cloud::aiplatform::v1::DeleteCustomJobRequest const& request);

  virtual future<
      StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>
  DeleteCustomJob(google::longrunning::Operation const& operation);

  virtual Status CancelCustomJob(
      google::cloud::aiplatform::v1::CancelCustomJobRequest const& request);

  virtual StatusOr<google::cloud::aiplatform::v1::DataLabelingJob>
  CreateDataLabelingJob(
      google::cloud::aiplatform::v1::CreateDataLabelingJobRequest const&
          request);

  virtual StatusOr<google::cloud::aiplatform::v1::DataLabelingJob>
  GetDataLabelingJob(
      google::cloud::aiplatform::v1::GetDataLabelingJobRequest const& request);

  virtual StreamRange<google::cloud::aiplatform::v1::DataLabelingJob>
  ListDataLabelingJobs(
      google::cloud::aiplatform::v1::ListDataLabelingJobsRequest request);

  virtual future<
      StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>
  DeleteDataLabelingJob(
      google::cloud::aiplatform::v1::DeleteDataLabelingJobRequest const&
          request);

  virtual StatusOr<google::longrunning::Operation> DeleteDataLabelingJob(
      NoAwaitTag,
      google::cloud::aiplatform::v1::DeleteDataLabelingJobRequest const&
          request);

  virtual future<
      StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>
  DeleteDataLabelingJob(google::longrunning::Operation const& operation);

  virtual Status CancelDataLabelingJob(
      google::cloud::aiplatform::v1::CancelDataLabelingJobRequest const&
          request);

  virtual StatusOr<google::cloud::aiplatform::v1::HyperparameterTuningJob>
  CreateHyperparameterTuningJob(
      google::cloud::aiplatform::v1::CreateHyperparameterTuningJobRequest const&
          request);

  virtual StatusOr<google::cloud::aiplatform::v1::HyperparameterTuningJob>
  GetHyperparameterTuningJob(
      google::cloud::aiplatform::v1::GetHyperparameterTuningJobRequest const&
          request);

  virtual StreamRange<google::cloud::aiplatform::v1::HyperparameterTuningJob>
  ListHyperparameterTuningJobs(
      google::cloud::aiplatform::v1::ListHyperparameterTuningJobsRequest
          request);

  virtual future<
      StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>
  DeleteHyperparameterTuningJob(
      google::cloud::aiplatform::v1::DeleteHyperparameterTuningJobRequest const&
          request);

  virtual StatusOr<google::longrunning::Operation>
  DeleteHyperparameterTuningJob(
      NoAwaitTag,
      google::cloud::aiplatform::v1::DeleteHyperparameterTuningJobRequest const&
          request);

  virtual future<
      StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>
  DeleteHyperparameterTuningJob(
      google::longrunning::Operation const& operation);

  virtual Status CancelHyperparameterTuningJob(
      google::cloud::aiplatform::v1::CancelHyperparameterTuningJobRequest const&
          request);

  virtual StatusOr<google::cloud::aiplatform::v1::NasJob> CreateNasJob(
      google::cloud::aiplatform::v1::CreateNasJobRequest const& request);

  virtual StatusOr<google::cloud::aiplatform::v1::NasJob> GetNasJob(
      google::cloud::aiplatform::v1::GetNasJobRequest const& request);

  virtual StreamRange<google::cloud::aiplatform::v1::NasJob> ListNasJobs(
      google::cloud::aiplatform::v1::ListNasJobsRequest request);

  virtual future<
      StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>
  DeleteNasJob(
      google::cloud::aiplatform::v1::DeleteNasJobRequest const& request);

  virtual StatusOr<google::longrunning::Operation> DeleteNasJob(
      NoAwaitTag,
      google::cloud::aiplatform::v1::DeleteNasJobRequest const& request);

  virtual future<
      StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>
  DeleteNasJob(google::longrunning::Operation const& operation);

  virtual Status CancelNasJob(
      google::cloud::aiplatform::v1::CancelNasJobRequest const& request);

  virtual StatusOr<google::cloud::aiplatform::v1::NasTrialDetail>
  GetNasTrialDetail(
      google::cloud::aiplatform::v1::GetNasTrialDetailRequest const& request);

  virtual StreamRange<google::cloud::aiplatform::v1::NasTrialDetail>
  ListNasTrialDetails(
      google::cloud::aiplatform::v1::ListNasTrialDetailsRequest request);

  virtual StatusOr<google::cloud::aiplatform::v1::BatchPredictionJob>
  CreateBatchPredictionJob(
      google::cloud::aiplatform::v1::CreateBatchPredictionJobRequest const&
          request);

  virtual StatusOr<google::cloud::aiplatform::v1::BatchPredictionJob>
  GetBatchPredictionJob(
      google::cloud::aiplatform::v1::GetBatchPredictionJobRequest const&
          request);

  virtual StreamRange<google::cloud::aiplatform::v1::BatchPredictionJob>
  ListBatchPredictionJobs(
      google::cloud::aiplatform::v1::ListBatchPredictionJobsRequest request);

  virtual future<
      StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>
  DeleteBatchPredictionJob(
      google::cloud::aiplatform::v1::DeleteBatchPredictionJobRequest const&
          request);

  virtual StatusOr<google::longrunning::Operation> DeleteBatchPredictionJob(
      NoAwaitTag,
      google::cloud::aiplatform::v1::DeleteBatchPredictionJobRequest const&
          request);

  virtual future<
      StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>
  DeleteBatchPredictionJob(google::longrunning::Operation const& operation);

  virtual Status CancelBatchPredictionJob(
      google::cloud::aiplatform::v1::CancelBatchPredictionJobRequest const&
          request);

  virtual StatusOr<google::cloud::aiplatform::v1::ModelDeploymentMonitoringJob>
  CreateModelDeploymentMonitoringJob(
      google::cloud::aiplatform::v1::
          CreateModelDeploymentMonitoringJobRequest const& request);

  virtual StreamRange<
      google::cloud::aiplatform::v1::ModelMonitoringStatsAnomalies>
  SearchModelDeploymentMonitoringStatsAnomalies(
      google::cloud::aiplatform::v1::
          SearchModelDeploymentMonitoringStatsAnomaliesRequest request);

  virtual StatusOr<google::cloud::aiplatform::v1::ModelDeploymentMonitoringJob>
  GetModelDeploymentMonitoringJob(
      google::cloud::aiplatform::v1::
          GetModelDeploymentMonitoringJobRequest const& request);

  virtual StreamRange<
      google::cloud::aiplatform::v1::ModelDeploymentMonitoringJob>
  ListModelDeploymentMonitoringJobs(
      google::cloud::aiplatform::v1::ListModelDeploymentMonitoringJobsRequest
          request);

  virtual future<
      StatusOr<google::cloud::aiplatform::v1::ModelDeploymentMonitoringJob>>
  UpdateModelDeploymentMonitoringJob(
      google::cloud::aiplatform::v1::
          UpdateModelDeploymentMonitoringJobRequest const& request);

  virtual StatusOr<google::longrunning::Operation>
  UpdateModelDeploymentMonitoringJob(
      NoAwaitTag, google::cloud::aiplatform::v1::
                      UpdateModelDeploymentMonitoringJobRequest const& request);

  virtual future<
      StatusOr<google::cloud::aiplatform::v1::ModelDeploymentMonitoringJob>>
  UpdateModelDeploymentMonitoringJob(
      google::longrunning::Operation const& operation);

  virtual future<
      StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>
  DeleteModelDeploymentMonitoringJob(
      google::cloud::aiplatform::v1::
          DeleteModelDeploymentMonitoringJobRequest const& request);

  virtual StatusOr<google::longrunning::Operation>
  DeleteModelDeploymentMonitoringJob(
      NoAwaitTag, google::cloud::aiplatform::v1::
                      DeleteModelDeploymentMonitoringJobRequest const& request);

  virtual future<
      StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>
  DeleteModelDeploymentMonitoringJob(
      google::longrunning::Operation const& operation);

  virtual Status PauseModelDeploymentMonitoringJob(
      google::cloud::aiplatform::v1::
          PauseModelDeploymentMonitoringJobRequest const& request);

  virtual Status ResumeModelDeploymentMonitoringJob(
      google::cloud::aiplatform::v1::
          ResumeModelDeploymentMonitoringJobRequest const& request);

  virtual StreamRange<google::cloud::location::Location> ListLocations(
      google::cloud::location::ListLocationsRequest request);

  virtual StatusOr<google::cloud::location::Location> GetLocation(
      google::cloud::location::GetLocationRequest const& request);

  virtual StatusOr<google::iam::v1::Policy> SetIamPolicy(
      google::iam::v1::SetIamPolicyRequest const& request);

  virtual StatusOr<google::iam::v1::Policy> GetIamPolicy(
      google::iam::v1::GetIamPolicyRequest const& request);

  virtual StatusOr<google::iam::v1::TestIamPermissionsResponse>
  TestIamPermissions(google::iam::v1::TestIamPermissionsRequest const& request);

  virtual StreamRange<google::longrunning::Operation> ListOperations(
      google::longrunning::ListOperationsRequest request);

  virtual StatusOr<google::longrunning::Operation> GetOperation(
      google::longrunning::GetOperationRequest const& request);

  virtual Status DeleteOperation(
      google::longrunning::DeleteOperationRequest const& request);

  virtual Status CancelOperation(
      google::longrunning::CancelOperationRequest const& request);

  virtual StatusOr<google::longrunning::Operation> WaitOperation(
      google::longrunning::WaitOperationRequest const& request);
};

/**
 * A factory function to construct an object of type `JobServiceConnection`.
 *
 * The returned connection object should not be used directly; instead it
 * should be passed as an argument to the constructor of JobServiceClient.
 *
 * The optional @p options argument may be used to configure aspects of the
 * returned `JobServiceConnection`. Expected options are any of the types in
 * the following option lists:
 *
 * - `google::cloud::CommonOptionList`
 * - `google::cloud::GrpcOptionList`
 * - `google::cloud::UnifiedCredentialsOptionList`
 * - `google::cloud::aiplatform_v1::JobServicePolicyOptionList`
 *
 * @note Unexpected options will be ignored. To log unexpected options instead,
 *     set `GOOGLE_CLOUD_CPP_ENABLE_CLOG=yes` in the environment.
 *
 * @param location Sets the prefix for the default `EndpointOption` value.
 * @param options (optional) Configure the `JobServiceConnection` created by
 * this function.
 */
std::shared_ptr<JobServiceConnection> MakeJobServiceConnection(
    std::string const& location, Options options = {});

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace aiplatform_v1
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_AIPLATFORM_V1_JOB_CONNECTION_H
