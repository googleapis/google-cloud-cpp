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
// source: google/cloud/eventarc/v1/eventarc.proto

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_EVENTARC_V1_MOCKS_MOCK_EVENTARC_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_EVENTARC_V1_MOCKS_MOCK_EVENTARC_CONNECTION_H

#include "google/cloud/eventarc/v1/eventarc_connection.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace eventarc_v1_mocks {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A class to mock `EventarcConnection`.
 *
 * Application developers may want to test their code with simulated responses,
 * including errors, from an object of type `EventarcClient`. To do so,
 * construct an object of type `EventarcClient` with an instance of this
 * class. Then use the Google Test framework functions to program the behavior
 * of this mock.
 *
 * @see [This example][bq-mock] for how to test your application with GoogleTest.
 * While the example showcases types from the BigQuery library, the underlying
 * principles apply for any pair of `*Client` and `*Connection`.
 *
 * [bq-mock]: @cloud_cpp_docs_link{bigquery,bigquery-read-mock}
 */
class MockEventarcConnection : public eventarc_v1::EventarcConnection {
 public:
  MOCK_METHOD(Options, options, (), (override));

  MOCK_METHOD(StatusOr<google::cloud::eventarc::v1::Trigger>, GetTrigger,
              (google::cloud::eventarc::v1::GetTriggerRequest const& request),
              (override));

  MOCK_METHOD((StreamRange<google::cloud::eventarc::v1::Trigger>), ListTriggers,
              (google::cloud::eventarc::v1::ListTriggersRequest request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// CreateTrigger(Matcher<google::cloud::eventarc::v1::CreateTriggerRequest
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(
      future<StatusOr<google::cloud::eventarc::v1::Trigger>>, CreateTrigger,
      (google::cloud::eventarc::v1::CreateTriggerRequest const& request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// EXPECT_CALL(*mock, CreateTrigger(_, _))
  /// @endcode
  MOCK_METHOD(
      StatusOr<google::longrunning::Operation>, CreateTrigger,
      (NoAwaitTag,
       google::cloud::eventarc::v1::CreateTriggerRequest const& request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock, CreateTrigger(Matcher<google::longrunning::Operation
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<google::cloud::eventarc::v1::Trigger>>,
              CreateTrigger, (google::longrunning::Operation const& operation),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// UpdateTrigger(Matcher<google::cloud::eventarc::v1::UpdateTriggerRequest
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(
      future<StatusOr<google::cloud::eventarc::v1::Trigger>>, UpdateTrigger,
      (google::cloud::eventarc::v1::UpdateTriggerRequest const& request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// EXPECT_CALL(*mock, UpdateTrigger(_, _))
  /// @endcode
  MOCK_METHOD(
      StatusOr<google::longrunning::Operation>, UpdateTrigger,
      (NoAwaitTag,
       google::cloud::eventarc::v1::UpdateTriggerRequest const& request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock, UpdateTrigger(Matcher<google::longrunning::Operation
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<google::cloud::eventarc::v1::Trigger>>,
              UpdateTrigger, (google::longrunning::Operation const& operation),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// DeleteTrigger(Matcher<google::cloud::eventarc::v1::DeleteTriggerRequest
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(
      future<StatusOr<google::cloud::eventarc::v1::Trigger>>, DeleteTrigger,
      (google::cloud::eventarc::v1::DeleteTriggerRequest const& request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// EXPECT_CALL(*mock, DeleteTrigger(_, _))
  /// @endcode
  MOCK_METHOD(
      StatusOr<google::longrunning::Operation>, DeleteTrigger,
      (NoAwaitTag,
       google::cloud::eventarc::v1::DeleteTriggerRequest const& request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock, DeleteTrigger(Matcher<google::longrunning::Operation
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<google::cloud::eventarc::v1::Trigger>>,
              DeleteTrigger, (google::longrunning::Operation const& operation),
              (override));

  MOCK_METHOD(StatusOr<google::cloud::eventarc::v1::Channel>, GetChannel,
              (google::cloud::eventarc::v1::GetChannelRequest const& request),
              (override));

  MOCK_METHOD((StreamRange<google::cloud::eventarc::v1::Channel>), ListChannels,
              (google::cloud::eventarc::v1::ListChannelsRequest request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// CreateChannel(Matcher<google::cloud::eventarc::v1::CreateChannelRequest
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(
      future<StatusOr<google::cloud::eventarc::v1::Channel>>, CreateChannel,
      (google::cloud::eventarc::v1::CreateChannelRequest const& request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// EXPECT_CALL(*mock, CreateChannel(_, _))
  /// @endcode
  MOCK_METHOD(
      StatusOr<google::longrunning::Operation>, CreateChannel,
      (NoAwaitTag,
       google::cloud::eventarc::v1::CreateChannelRequest const& request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock, CreateChannel(Matcher<google::longrunning::Operation
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<google::cloud::eventarc::v1::Channel>>,
              CreateChannel, (google::longrunning::Operation const& operation),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// UpdateChannel(Matcher<google::cloud::eventarc::v1::UpdateChannelRequest
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(
      future<StatusOr<google::cloud::eventarc::v1::Channel>>, UpdateChannel,
      (google::cloud::eventarc::v1::UpdateChannelRequest const& request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// EXPECT_CALL(*mock, UpdateChannel(_, _))
  /// @endcode
  MOCK_METHOD(
      StatusOr<google::longrunning::Operation>, UpdateChannel,
      (NoAwaitTag,
       google::cloud::eventarc::v1::UpdateChannelRequest const& request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock, UpdateChannel(Matcher<google::longrunning::Operation
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<google::cloud::eventarc::v1::Channel>>,
              UpdateChannel, (google::longrunning::Operation const& operation),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// DeleteChannel(Matcher<google::cloud::eventarc::v1::DeleteChannelRequest
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(
      future<StatusOr<google::cloud::eventarc::v1::Channel>>, DeleteChannel,
      (google::cloud::eventarc::v1::DeleteChannelRequest const& request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// EXPECT_CALL(*mock, DeleteChannel(_, _))
  /// @endcode
  MOCK_METHOD(
      StatusOr<google::longrunning::Operation>, DeleteChannel,
      (NoAwaitTag,
       google::cloud::eventarc::v1::DeleteChannelRequest const& request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock, DeleteChannel(Matcher<google::longrunning::Operation
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<google::cloud::eventarc::v1::Channel>>,
              DeleteChannel, (google::longrunning::Operation const& operation),
              (override));

  MOCK_METHOD(StatusOr<google::cloud::eventarc::v1::Provider>, GetProvider,
              (google::cloud::eventarc::v1::GetProviderRequest const& request),
              (override));

  MOCK_METHOD((StreamRange<google::cloud::eventarc::v1::Provider>),
              ListProviders,
              (google::cloud::eventarc::v1::ListProvidersRequest request),
              (override));

  MOCK_METHOD(
      StatusOr<google::cloud::eventarc::v1::ChannelConnection>,
      GetChannelConnection,
      (google::cloud::eventarc::v1::GetChannelConnectionRequest const& request),
      (override));

  MOCK_METHOD(
      (StreamRange<google::cloud::eventarc::v1::ChannelConnection>),
      ListChannelConnections,
      (google::cloud::eventarc::v1::ListChannelConnectionsRequest request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// CreateChannelConnection(Matcher<google::cloud::eventarc::v1::CreateChannelConnectionRequest
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(
      future<StatusOr<google::cloud::eventarc::v1::ChannelConnection>>,
      CreateChannelConnection,
      (google::cloud::eventarc::v1::CreateChannelConnectionRequest const&
           request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// EXPECT_CALL(*mock, CreateChannelConnection(_, _))
  /// @endcode
  MOCK_METHOD(
      StatusOr<google::longrunning::Operation>, CreateChannelConnection,
      (NoAwaitTag,
       google::cloud::eventarc::v1::CreateChannelConnectionRequest const&
           request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// CreateChannelConnection(Matcher<google::longrunning::Operation
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<google::cloud::eventarc::v1::ChannelConnection>>,
              CreateChannelConnection,
              (google::longrunning::Operation const& operation), (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// DeleteChannelConnection(Matcher<google::cloud::eventarc::v1::DeleteChannelConnectionRequest
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(
      future<StatusOr<google::cloud::eventarc::v1::ChannelConnection>>,
      DeleteChannelConnection,
      (google::cloud::eventarc::v1::DeleteChannelConnectionRequest const&
           request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// EXPECT_CALL(*mock, DeleteChannelConnection(_, _))
  /// @endcode
  MOCK_METHOD(
      StatusOr<google::longrunning::Operation>, DeleteChannelConnection,
      (NoAwaitTag,
       google::cloud::eventarc::v1::DeleteChannelConnectionRequest const&
           request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// DeleteChannelConnection(Matcher<google::longrunning::Operation
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<google::cloud::eventarc::v1::ChannelConnection>>,
              DeleteChannelConnection,
              (google::longrunning::Operation const& operation), (override));

  MOCK_METHOD(StatusOr<google::cloud::eventarc::v1::GoogleChannelConfig>,
              GetGoogleChannelConfig,
              (google::cloud::eventarc::v1::GetGoogleChannelConfigRequest const&
                   request),
              (override));

  MOCK_METHOD(
      StatusOr<google::cloud::eventarc::v1::GoogleChannelConfig>,
      UpdateGoogleChannelConfig,
      (google::cloud::eventarc::v1::UpdateGoogleChannelConfigRequest const&
           request),
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
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace eventarc_v1_mocks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_EVENTARC_V1_MOCKS_MOCK_EVENTARC_CONNECTION_H
