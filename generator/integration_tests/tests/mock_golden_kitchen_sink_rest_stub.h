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

#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTEGRATION_TESTS_TESTS_MOCK_GOLDEN_KITCHEN_SINK_REST_STUB_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTEGRATION_TESTS_TESTS_MOCK_GOLDEN_KITCHEN_SINK_REST_STUB_H

#include "generator/integration_tests/golden/v1/internal/golden_kitchen_sink_rest_stub.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace golden_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class MockGoldenKitchenSinkRestStub : public GoldenKitchenSinkRestStub {
 public:
  ~MockGoldenKitchenSinkRestStub() override = default;

  MOCK_METHOD(
      StatusOr<
          ::google::test::admin::database::v1::GenerateAccessTokenResponse>,
      GenerateAccessToken,
      (google::cloud::rest_internal::RestContext&,
       ::google::test::admin::database::v1::GenerateAccessTokenRequest const&),
      (override));
  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::GenerateIdTokenResponse>,
      GenerateIdToken,
      (google::cloud::rest_internal::RestContext&,
       ::google::test::admin::database::v1::GenerateIdTokenRequest const&),
      (override));
  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::WriteLogEntriesResponse>,
      WriteLogEntries,
      (google::cloud::rest_internal::RestContext&,
       ::google::test::admin::database::v1::WriteLogEntriesRequest const&),
      (override));
  MOCK_METHOD(StatusOr<::google::test::admin::database::v1::ListLogsResponse>,
              ListLogs,
              (google::cloud::rest_internal::RestContext&,
               ::google::test::admin::database::v1::ListLogsRequest const&),
              (override));
  MOCK_METHOD(
      StatusOr<
          ::google::test::admin::database::v1::ListServiceAccountKeysResponse>,
      ListServiceAccountKeys,
      (google::cloud::rest_internal::RestContext&,
       ::google::test::admin::database::v1::
           ListServiceAccountKeysRequest const&),
      (override));
  MOCK_METHOD(Status, DoNothing,
              (google::cloud::rest_internal::RestContext&,
               ::google::protobuf::Empty const&),
              (override));
  MOCK_METHOD(
      Status, ExplicitRouting1,
      (google::cloud::rest_internal::RestContext&,
       ::google::test::admin::database::v1::ExplicitRoutingRequest const&),
      (override));

  MOCK_METHOD(
      Status, ExplicitRouting2,
      (google::cloud::rest_internal::RestContext&,
       ::google::test::admin::database::v1::ExplicitRoutingRequest const&),
      (override));
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTEGRATION_TESTS_TESTS_MOCK_GOLDEN_KITCHEN_SINK_REST_STUB_H
