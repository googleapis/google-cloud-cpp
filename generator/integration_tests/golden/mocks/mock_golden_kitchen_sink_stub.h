// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTEGRATION_TESTS_GOLDEN_MOCKS_MOCK_GOLDEN_KITCHEN_SINK_STUB_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTEGRATION_TESTS_GOLDEN_MOCKS_MOCK_GOLDEN_KITCHEN_SINK_STUB_H

#include "generator/integration_tests/golden/internal/golden_kitchen_sink_stub.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace golden_internal {
inline namespace GOOGLE_CLOUD_CPP_GENERATED_NS {

class MockGoldenKitchenSinkStub : public GoldenKitchenSinkStub {
 public:
  ~MockGoldenKitchenSinkStub() override = default;

  MOCK_METHOD(
      StatusOr<
          ::google::test::admin::database::v1::GenerateAccessTokenResponse>,
      GenerateAccessToken,
      (grpc::ClientContext&,
       ::google::test::admin::database::v1::GenerateAccessTokenRequest const&),
      (override));
  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::GenerateIdTokenResponse>,
      GenerateIdToken,
      (grpc::ClientContext&,
       ::google::test::admin::database::v1::GenerateIdTokenRequest const&),
      (override));
  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::WriteLogEntriesResponse>,
      WriteLogEntries,
      (grpc::ClientContext&,
       ::google::test::admin::database::v1::WriteLogEntriesRequest const&),
      (override));
  MOCK_METHOD(StatusOr<::google::test::admin::database::v1::ListLogsResponse>,
              ListLogs,
              (grpc::ClientContext&,
               ::google::test::admin::database::v1::ListLogsRequest const&),
              (override));
  MOCK_METHOD(
      (std::unique_ptr<internal::StreamingReadRpc<
           ::google::test::admin::database::v1::TailLogEntriesResponse>>),
      TailLogEntries,
      (std::unique_ptr<grpc::ClientContext> context,
       ::google::test::admin::database::v1::TailLogEntriesRequest const&),
      (override));
  MOCK_METHOD(
      StatusOr<
          ::google::test::admin::database::v1::ListServiceAccountKeysResponse>,
      ListServiceAccountKeys,
      (grpc::ClientContext&, ::google::test::admin::database::v1::
                                 ListServiceAccountKeysRequest const&),
      (override));
};

class MockTailLogEntriesStreamingReadRpc
    : public internal::StreamingReadRpc<::google::test::admin::database::v1::TailLogEntriesResponse> {
public:
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD((absl::variant<Status, ::google::test::admin::database::v1::TailLogEntriesResponse>), Read, (),
  (override));
  MOCK_METHOD(internal::StreamingRpcMetadata, GetRequestMetadata, (),
  (const, override));
};

}  // namespace GOOGLE_CLOUD_CPP_GENERATED_NS
}  // namespace golden_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTEGRATION_TESTS_GOLDEN_MOCKS_MOCK_GOLDEN_KITCHEN_SINK_STUB_H
