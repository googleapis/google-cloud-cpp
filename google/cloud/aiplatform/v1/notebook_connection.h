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
// source: google/cloud/aiplatform/v1/notebook_service.proto

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_AIPLATFORM_V1_NOTEBOOK_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_AIPLATFORM_V1_NOTEBOOK_CONNECTION_H

#include "google/cloud/aiplatform/v1/internal/notebook_retry_traits.h"
#include "google/cloud/aiplatform/v1/notebook_connection_idempotency_policy.h"
#include "google/cloud/backoff_policy.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/retry_policy_impl.h"
#include "google/cloud/no_await_tag.h"
#include "google/cloud/options.h"
#include "google/cloud/polling_policy.h"
#include "google/cloud/status_or.h"
#include "google/cloud/stream_range.h"
#include "google/cloud/version.h"
#include <google/cloud/aiplatform/v1/notebook_service.pb.h>
#include <google/longrunning/operations.grpc.pb.h>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace aiplatform_v1 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// The retry policy for `NotebookServiceConnection`.
class NotebookServiceRetryPolicy : public ::google::cloud::RetryPolicy {
 public:
  /// Creates a new instance of the policy, reset to the initial state.
  virtual std::unique_ptr<NotebookServiceRetryPolicy> clone() const = 0;
};

/**
 * A retry policy for `NotebookServiceConnection` based on counting errors.
 *
 * This policy stops retrying if:
 * - An RPC returns a non-transient error.
 * - More than a prescribed number of transient failures is detected.
 *
 * In this class the following status codes are treated as transient errors:
 * - [`kUnavailable`](@ref google::cloud::StatusCode)
 */
class NotebookServiceLimitedErrorCountRetryPolicy
    : public NotebookServiceRetryPolicy {
 public:
  /**
   * Create an instance that tolerates up to @p maximum_failures transient
   * errors.
   *
   * @note Disable the retry loop by providing an instance of this policy with
   *     @p maximum_failures == 0.
   */
  explicit NotebookServiceLimitedErrorCountRetryPolicy(int maximum_failures)
      : impl_(maximum_failures) {}

  NotebookServiceLimitedErrorCountRetryPolicy(
      NotebookServiceLimitedErrorCountRetryPolicy&& rhs) noexcept
      : NotebookServiceLimitedErrorCountRetryPolicy(rhs.maximum_failures()) {}
  NotebookServiceLimitedErrorCountRetryPolicy(
      NotebookServiceLimitedErrorCountRetryPolicy const& rhs) noexcept
      : NotebookServiceLimitedErrorCountRetryPolicy(rhs.maximum_failures()) {}

  int maximum_failures() const { return impl_.maximum_failures(); }

  bool OnFailure(Status const& status) override {
    return impl_.OnFailure(status);
  }
  bool IsExhausted() const override { return impl_.IsExhausted(); }
  bool IsPermanentFailure(Status const& status) const override {
    return impl_.IsPermanentFailure(status);
  }
  std::unique_ptr<NotebookServiceRetryPolicy> clone() const override {
    return std::make_unique<NotebookServiceLimitedErrorCountRetryPolicy>(
        maximum_failures());
  }

  // This is provided only for backwards compatibility.
  using BaseType = NotebookServiceRetryPolicy;

 private:
  google::cloud::internal::LimitedErrorCountRetryPolicy<
      aiplatform_v1_internal::NotebookServiceRetryTraits>
      impl_;
};

/**
 * A retry policy for `NotebookServiceConnection` based on elapsed time.
 *
 * This policy stops retrying if:
 * - An RPC returns a non-transient error.
 * - The elapsed time in the retry loop exceeds a prescribed duration.
 *
 * In this class the following status codes are treated as transient errors:
 * - [`kUnavailable`](@ref google::cloud::StatusCode)
 */
class NotebookServiceLimitedTimeRetryPolicy
    : public NotebookServiceRetryPolicy {
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
  explicit NotebookServiceLimitedTimeRetryPolicy(
      std::chrono::duration<DurationRep, DurationPeriod> maximum_duration)
      : impl_(maximum_duration) {}

  NotebookServiceLimitedTimeRetryPolicy(
      NotebookServiceLimitedTimeRetryPolicy&& rhs) noexcept
      : NotebookServiceLimitedTimeRetryPolicy(rhs.maximum_duration()) {}
  NotebookServiceLimitedTimeRetryPolicy(
      NotebookServiceLimitedTimeRetryPolicy const& rhs) noexcept
      : NotebookServiceLimitedTimeRetryPolicy(rhs.maximum_duration()) {}

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
  std::unique_ptr<NotebookServiceRetryPolicy> clone() const override {
    return std::make_unique<NotebookServiceLimitedTimeRetryPolicy>(
        maximum_duration());
  }

  // This is provided only for backwards compatibility.
  using BaseType = NotebookServiceRetryPolicy;

 private:
  google::cloud::internal::LimitedTimeRetryPolicy<
      aiplatform_v1_internal::NotebookServiceRetryTraits>
      impl_;
};

/**
 * The `NotebookServiceConnection` object for `NotebookServiceClient`.
 *
 * This interface defines virtual methods for each of the user-facing overload
 * sets in `NotebookServiceClient`. This allows users to inject custom behavior
 * (e.g., with a Google Mock object) when writing tests that use objects of type
 * `NotebookServiceClient`.
 *
 * To create a concrete instance, see `MakeNotebookServiceConnection()`.
 *
 * For mocking, see `aiplatform_v1_mocks::MockNotebookServiceConnection`.
 */
class NotebookServiceConnection {
 public:
  virtual ~NotebookServiceConnection() = 0;

  virtual Options options() { return Options{}; }

  virtual future<
      StatusOr<google::cloud::aiplatform::v1::NotebookRuntimeTemplate>>
  CreateNotebookRuntimeTemplate(
      google::cloud::aiplatform::v1::CreateNotebookRuntimeTemplateRequest const&
          request);

  virtual StatusOr<google::longrunning::Operation>
  CreateNotebookRuntimeTemplate(
      NoAwaitTag,
      google::cloud::aiplatform::v1::CreateNotebookRuntimeTemplateRequest const&
          request);

  virtual future<
      StatusOr<google::cloud::aiplatform::v1::NotebookRuntimeTemplate>>
  CreateNotebookRuntimeTemplate(
      google::longrunning::Operation const& operation);

  virtual StatusOr<google::cloud::aiplatform::v1::NotebookRuntimeTemplate>
  GetNotebookRuntimeTemplate(
      google::cloud::aiplatform::v1::GetNotebookRuntimeTemplateRequest const&
          request);

  virtual StreamRange<google::cloud::aiplatform::v1::NotebookRuntimeTemplate>
  ListNotebookRuntimeTemplates(
      google::cloud::aiplatform::v1::ListNotebookRuntimeTemplatesRequest
          request);

  virtual future<
      StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>
  DeleteNotebookRuntimeTemplate(
      google::cloud::aiplatform::v1::DeleteNotebookRuntimeTemplateRequest const&
          request);

  virtual StatusOr<google::longrunning::Operation>
  DeleteNotebookRuntimeTemplate(
      NoAwaitTag,
      google::cloud::aiplatform::v1::DeleteNotebookRuntimeTemplateRequest const&
          request);

  virtual future<
      StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>
  DeleteNotebookRuntimeTemplate(
      google::longrunning::Operation const& operation);

  virtual StatusOr<google::cloud::aiplatform::v1::NotebookRuntimeTemplate>
  UpdateNotebookRuntimeTemplate(
      google::cloud::aiplatform::v1::UpdateNotebookRuntimeTemplateRequest const&
          request);

  virtual future<StatusOr<google::cloud::aiplatform::v1::NotebookRuntime>>
  AssignNotebookRuntime(
      google::cloud::aiplatform::v1::AssignNotebookRuntimeRequest const&
          request);

  virtual StatusOr<google::longrunning::Operation> AssignNotebookRuntime(
      NoAwaitTag,
      google::cloud::aiplatform::v1::AssignNotebookRuntimeRequest const&
          request);

  virtual future<StatusOr<google::cloud::aiplatform::v1::NotebookRuntime>>
  AssignNotebookRuntime(google::longrunning::Operation const& operation);

  virtual StatusOr<google::cloud::aiplatform::v1::NotebookRuntime>
  GetNotebookRuntime(
      google::cloud::aiplatform::v1::GetNotebookRuntimeRequest const& request);

  virtual StreamRange<google::cloud::aiplatform::v1::NotebookRuntime>
  ListNotebookRuntimes(
      google::cloud::aiplatform::v1::ListNotebookRuntimesRequest request);

  virtual future<
      StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>
  DeleteNotebookRuntime(
      google::cloud::aiplatform::v1::DeleteNotebookRuntimeRequest const&
          request);

  virtual StatusOr<google::longrunning::Operation> DeleteNotebookRuntime(
      NoAwaitTag,
      google::cloud::aiplatform::v1::DeleteNotebookRuntimeRequest const&
          request);

  virtual future<
      StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>
  DeleteNotebookRuntime(google::longrunning::Operation const& operation);

  virtual future<
      StatusOr<google::cloud::aiplatform::v1::UpgradeNotebookRuntimeResponse>>
  UpgradeNotebookRuntime(
      google::cloud::aiplatform::v1::UpgradeNotebookRuntimeRequest const&
          request);

  virtual StatusOr<google::longrunning::Operation> UpgradeNotebookRuntime(
      NoAwaitTag,
      google::cloud::aiplatform::v1::UpgradeNotebookRuntimeRequest const&
          request);

  virtual future<
      StatusOr<google::cloud::aiplatform::v1::UpgradeNotebookRuntimeResponse>>
  UpgradeNotebookRuntime(google::longrunning::Operation const& operation);

  virtual future<
      StatusOr<google::cloud::aiplatform::v1::StartNotebookRuntimeResponse>>
  StartNotebookRuntime(
      google::cloud::aiplatform::v1::StartNotebookRuntimeRequest const&
          request);

  virtual StatusOr<google::longrunning::Operation> StartNotebookRuntime(
      NoAwaitTag,
      google::cloud::aiplatform::v1::StartNotebookRuntimeRequest const&
          request);

  virtual future<
      StatusOr<google::cloud::aiplatform::v1::StartNotebookRuntimeResponse>>
  StartNotebookRuntime(google::longrunning::Operation const& operation);

  virtual future<
      StatusOr<google::cloud::aiplatform::v1::StopNotebookRuntimeResponse>>
  StopNotebookRuntime(
      google::cloud::aiplatform::v1::StopNotebookRuntimeRequest const& request);

  virtual StatusOr<google::longrunning::Operation> StopNotebookRuntime(
      NoAwaitTag,
      google::cloud::aiplatform::v1::StopNotebookRuntimeRequest const& request);

  virtual future<
      StatusOr<google::cloud::aiplatform::v1::StopNotebookRuntimeResponse>>
  StopNotebookRuntime(google::longrunning::Operation const& operation);

  virtual future<StatusOr<google::cloud::aiplatform::v1::NotebookExecutionJob>>
  CreateNotebookExecutionJob(
      google::cloud::aiplatform::v1::CreateNotebookExecutionJobRequest const&
          request);

  virtual StatusOr<google::longrunning::Operation> CreateNotebookExecutionJob(
      NoAwaitTag,
      google::cloud::aiplatform::v1::CreateNotebookExecutionJobRequest const&
          request);

  virtual future<StatusOr<google::cloud::aiplatform::v1::NotebookExecutionJob>>
  CreateNotebookExecutionJob(google::longrunning::Operation const& operation);

  virtual StatusOr<google::cloud::aiplatform::v1::NotebookExecutionJob>
  GetNotebookExecutionJob(
      google::cloud::aiplatform::v1::GetNotebookExecutionJobRequest const&
          request);

  virtual StreamRange<google::cloud::aiplatform::v1::NotebookExecutionJob>
  ListNotebookExecutionJobs(
      google::cloud::aiplatform::v1::ListNotebookExecutionJobsRequest request);

  virtual future<
      StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>
  DeleteNotebookExecutionJob(
      google::cloud::aiplatform::v1::DeleteNotebookExecutionJobRequest const&
          request);

  virtual StatusOr<google::longrunning::Operation> DeleteNotebookExecutionJob(
      NoAwaitTag,
      google::cloud::aiplatform::v1::DeleteNotebookExecutionJobRequest const&
          request);

  virtual future<
      StatusOr<google::cloud::aiplatform::v1::DeleteOperationMetadata>>
  DeleteNotebookExecutionJob(google::longrunning::Operation const& operation);

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
 * A factory function to construct an object of type
 * `NotebookServiceConnection`.
 *
 * The returned connection object should not be used directly; instead it
 * should be passed as an argument to the constructor of NotebookServiceClient.
 *
 * The optional @p options argument may be used to configure aspects of the
 * returned `NotebookServiceConnection`. Expected options are any of the types
 * in the following option lists:
 *
 * - `google::cloud::CommonOptionList`
 * - `google::cloud::GrpcOptionList`
 * - `google::cloud::UnifiedCredentialsOptionList`
 * - `google::cloud::aiplatform_v1::NotebookServicePolicyOptionList`
 *
 * @note Unexpected options will be ignored. To log unexpected options instead,
 *     set `GOOGLE_CLOUD_CPP_ENABLE_CLOG=yes` in the environment.
 *
 * @param location Sets the prefix for the default `EndpointOption` value.
 * @param options (optional) Configure the `NotebookServiceConnection` created
 * by this function.
 */
std::shared_ptr<NotebookServiceConnection> MakeNotebookServiceConnection(
    std::string const& location, Options options = {});

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace aiplatform_v1
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_AIPLATFORM_V1_NOTEBOOK_CONNECTION_H
