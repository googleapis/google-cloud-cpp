// Copyright 2022 Google LLC
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
// source: google/cloud/connectors/v1/connectors_service.proto

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_CONNECTORS_V1_CONNECTORS_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_CONNECTORS_V1_CONNECTORS_CONNECTION_H

#include "google/cloud/connectors/v1/connectors_connection_idempotency_policy.h"
#include "google/cloud/connectors/v1/internal/connectors_retry_traits.h"
#include "google/cloud/backoff_policy.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/retry_policy_impl.h"
#include "google/cloud/no_await_tag.h"
#include "google/cloud/options.h"
#include "google/cloud/polling_policy.h"
#include "google/cloud/status_or.h"
#include "google/cloud/stream_range.h"
#include "google/cloud/version.h"
#include <google/cloud/connectors/v1/connectors_service.pb.h>
#include <google/longrunning/operations.grpc.pb.h>
#include <memory>

namespace google {
namespace cloud {
namespace connectors_v1 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// The retry policy for `ConnectorsConnection`.
class ConnectorsRetryPolicy : public ::google::cloud::RetryPolicy {
 public:
  /// Creates a new instance of the policy, reset to the initial state.
  virtual std::unique_ptr<ConnectorsRetryPolicy> clone() const = 0;
};

/**
 * A retry policy for `ConnectorsConnection` based on counting errors.
 *
 * This policy stops retrying if:
 * - An RPC returns a non-transient error.
 * - More than a prescribed number of transient failures is detected.
 *
 * In this class the following status codes are treated as transient errors:
 * - [`kUnavailable`](@ref google::cloud::StatusCode)
 */
class ConnectorsLimitedErrorCountRetryPolicy : public ConnectorsRetryPolicy {
 public:
  /**
   * Create an instance that tolerates up to @p maximum_failures transient
   * errors.
   *
   * @note Disable the retry loop by providing an instance of this policy with
   *     @p maximum_failures == 0.
   */
  explicit ConnectorsLimitedErrorCountRetryPolicy(int maximum_failures)
      : impl_(maximum_failures) {}

  ConnectorsLimitedErrorCountRetryPolicy(
      ConnectorsLimitedErrorCountRetryPolicy&& rhs) noexcept
      : ConnectorsLimitedErrorCountRetryPolicy(rhs.maximum_failures()) {}
  ConnectorsLimitedErrorCountRetryPolicy(
      ConnectorsLimitedErrorCountRetryPolicy const& rhs) noexcept
      : ConnectorsLimitedErrorCountRetryPolicy(rhs.maximum_failures()) {}

  int maximum_failures() const { return impl_.maximum_failures(); }

  bool OnFailure(Status const& status) override {
    return impl_.OnFailure(status);
  }
  bool IsExhausted() const override { return impl_.IsExhausted(); }
  bool IsPermanentFailure(Status const& status) const override {
    return impl_.IsPermanentFailure(status);
  }
  std::unique_ptr<ConnectorsRetryPolicy> clone() const override {
    return std::make_unique<ConnectorsLimitedErrorCountRetryPolicy>(
        maximum_failures());
  }

  // This is provided only for backwards compatibility.
  using BaseType = ConnectorsRetryPolicy;

 private:
  google::cloud::internal::LimitedErrorCountRetryPolicy<
      connectors_v1_internal::ConnectorsRetryTraits>
      impl_;
};

/**
 * A retry policy for `ConnectorsConnection` based on elapsed time.
 *
 * This policy stops retrying if:
 * - An RPC returns a non-transient error.
 * - The elapsed time in the retry loop exceeds a prescribed duration.
 *
 * In this class the following status codes are treated as transient errors:
 * - [`kUnavailable`](@ref google::cloud::StatusCode)
 */
class ConnectorsLimitedTimeRetryPolicy : public ConnectorsRetryPolicy {
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
  explicit ConnectorsLimitedTimeRetryPolicy(
      std::chrono::duration<DurationRep, DurationPeriod> maximum_duration)
      : impl_(maximum_duration) {}

  ConnectorsLimitedTimeRetryPolicy(
      ConnectorsLimitedTimeRetryPolicy&& rhs) noexcept
      : ConnectorsLimitedTimeRetryPolicy(rhs.maximum_duration()) {}
  ConnectorsLimitedTimeRetryPolicy(
      ConnectorsLimitedTimeRetryPolicy const& rhs) noexcept
      : ConnectorsLimitedTimeRetryPolicy(rhs.maximum_duration()) {}

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
  std::unique_ptr<ConnectorsRetryPolicy> clone() const override {
    return std::make_unique<ConnectorsLimitedTimeRetryPolicy>(
        maximum_duration());
  }

  // This is provided only for backwards compatibility.
  using BaseType = ConnectorsRetryPolicy;

 private:
  google::cloud::internal::LimitedTimeRetryPolicy<
      connectors_v1_internal::ConnectorsRetryTraits>
      impl_;
};

/**
 * The `ConnectorsConnection` object for `ConnectorsClient`.
 *
 * This interface defines virtual methods for each of the user-facing overload
 * sets in `ConnectorsClient`. This allows users to inject custom behavior
 * (e.g., with a Google Mock object) when writing tests that use objects of type
 * `ConnectorsClient`.
 *
 * To create a concrete instance, see `MakeConnectorsConnection()`.
 *
 * For mocking, see `connectors_v1_mocks::MockConnectorsConnection`.
 */
class ConnectorsConnection {
 public:
  virtual ~ConnectorsConnection() = 0;

  virtual Options options() { return Options{}; }

  virtual StreamRange<google::cloud::connectors::v1::Connection>
  ListConnections(
      google::cloud::connectors::v1::ListConnectionsRequest request);

  virtual StatusOr<google::cloud::connectors::v1::Connection> GetConnection(
      google::cloud::connectors::v1::GetConnectionRequest const& request);

  virtual future<StatusOr<google::cloud::connectors::v1::Connection>>
  CreateConnection(
      google::cloud::connectors::v1::CreateConnectionRequest const& request);

  virtual StatusOr<google::longrunning::Operation> CreateConnection(
      NoAwaitTag,
      google::cloud::connectors::v1::CreateConnectionRequest const& request);

  virtual future<StatusOr<google::cloud::connectors::v1::Connection>>
  CreateConnection(google::longrunning::Operation const& operation);

  virtual future<StatusOr<google::cloud::connectors::v1::Connection>>
  UpdateConnection(
      google::cloud::connectors::v1::UpdateConnectionRequest const& request);

  virtual StatusOr<google::longrunning::Operation> UpdateConnection(
      NoAwaitTag,
      google::cloud::connectors::v1::UpdateConnectionRequest const& request);

  virtual future<StatusOr<google::cloud::connectors::v1::Connection>>
  UpdateConnection(google::longrunning::Operation const& operation);

  virtual future<StatusOr<google::cloud::connectors::v1::OperationMetadata>>
  DeleteConnection(
      google::cloud::connectors::v1::DeleteConnectionRequest const& request);

  virtual StatusOr<google::longrunning::Operation> DeleteConnection(
      NoAwaitTag,
      google::cloud::connectors::v1::DeleteConnectionRequest const& request);

  virtual future<StatusOr<google::cloud::connectors::v1::OperationMetadata>>
  DeleteConnection(google::longrunning::Operation const& operation);

  virtual StreamRange<google::cloud::connectors::v1::Provider> ListProviders(
      google::cloud::connectors::v1::ListProvidersRequest request);

  virtual StatusOr<google::cloud::connectors::v1::Provider> GetProvider(
      google::cloud::connectors::v1::GetProviderRequest const& request);

  virtual StreamRange<google::cloud::connectors::v1::Connector> ListConnectors(
      google::cloud::connectors::v1::ListConnectorsRequest request);

  virtual StatusOr<google::cloud::connectors::v1::Connector> GetConnector(
      google::cloud::connectors::v1::GetConnectorRequest const& request);

  virtual StreamRange<google::cloud::connectors::v1::ConnectorVersion>
  ListConnectorVersions(
      google::cloud::connectors::v1::ListConnectorVersionsRequest request);

  virtual StatusOr<google::cloud::connectors::v1::ConnectorVersion>
  GetConnectorVersion(
      google::cloud::connectors::v1::GetConnectorVersionRequest const& request);

  virtual StatusOr<google::cloud::connectors::v1::ConnectionSchemaMetadata>
  GetConnectionSchemaMetadata(
      google::cloud::connectors::v1::GetConnectionSchemaMetadataRequest const&
          request);

  virtual future<
      StatusOr<google::cloud::connectors::v1::ConnectionSchemaMetadata>>
  RefreshConnectionSchemaMetadata(
      google::cloud::connectors::v1::
          RefreshConnectionSchemaMetadataRequest const& request);

  virtual StatusOr<google::longrunning::Operation>
  RefreshConnectionSchemaMetadata(
      NoAwaitTag, google::cloud::connectors::v1::
                      RefreshConnectionSchemaMetadataRequest const& request);

  virtual future<
      StatusOr<google::cloud::connectors::v1::ConnectionSchemaMetadata>>
  RefreshConnectionSchemaMetadata(
      google::longrunning::Operation const& operation);

  virtual StreamRange<google::cloud::connectors::v1::RuntimeEntitySchema>
  ListRuntimeEntitySchemas(
      google::cloud::connectors::v1::ListRuntimeEntitySchemasRequest request);

  virtual StreamRange<google::cloud::connectors::v1::RuntimeActionSchema>
  ListRuntimeActionSchemas(
      google::cloud::connectors::v1::ListRuntimeActionSchemasRequest request);

  virtual StatusOr<google::cloud::connectors::v1::RuntimeConfig>
  GetRuntimeConfig(
      google::cloud::connectors::v1::GetRuntimeConfigRequest const& request);

  virtual StatusOr<google::cloud::connectors::v1::Settings> GetGlobalSettings(
      google::cloud::connectors::v1::GetGlobalSettingsRequest const& request);

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
};

/**
 * A factory function to construct an object of type `ConnectorsConnection`.
 *
 * The returned connection object should not be used directly; instead it
 * should be passed as an argument to the constructor of ConnectorsClient.
 *
 * The optional @p options argument may be used to configure aspects of the
 * returned `ConnectorsConnection`. Expected options are any of the types in
 * the following option lists:
 *
 * - `google::cloud::CommonOptionList`
 * - `google::cloud::GrpcOptionList`
 * - `google::cloud::UnifiedCredentialsOptionList`
 * - `google::cloud::connectors_v1::ConnectorsPolicyOptionList`
 *
 * @note Unexpected options will be ignored. To log unexpected options instead,
 *     set `GOOGLE_CLOUD_CPP_ENABLE_CLOG=yes` in the environment.
 *
 * @param options (optional) Configure the `ConnectorsConnection` created by
 * this function.
 */
std::shared_ptr<ConnectorsConnection> MakeConnectorsConnection(
    Options options = {});

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace connectors_v1
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_CONNECTORS_V1_CONNECTORS_CONNECTION_H
