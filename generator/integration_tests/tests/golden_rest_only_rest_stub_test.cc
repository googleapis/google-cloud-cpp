// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "generator/integration_tests/golden/v1/internal/golden_rest_only_rest_stub.h"
#include "google/cloud/internal/rest_context.h"
#include "google/cloud/testing_util/mock_http_payload.h"
#include "google/cloud/testing_util/mock_rest_client.h"
#include "google/cloud/testing_util/mock_rest_response.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::rest_internal::HttpStatusCode;
using ::google::cloud::rest_internal::RestContext;
using ::google::cloud::rest_internal::RestRequest;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::MakeMockHttpPayloadSuccess;
using ::google::cloud::testing_util::MockRestClient;
using ::google::cloud::testing_util::MockRestResponse;
using ::testing::_;
using ::testing::A;
using ::testing::Eq;

TEST(GoldenRestOnlyRestStubTest, Noop) {
  auto mock_rest_client = std::make_unique<MockRestClient>();
  std::string const json_response = R"({})";
  RestContext rest_context;
  google::protobuf::Empty proto_request;

  auto mock_200_response = std::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_200_response, StatusCode()).WillOnce([] {
    return HttpStatusCode::kOk;
  });
  EXPECT_CALL(std::move(*mock_200_response), ExtractPayload).WillOnce([&] {
    return MakeMockHttpPayloadSuccess(json_response);
  });
  EXPECT_CALL(*mock_rest_client,
              Post(_, _, A<std::vector<absl::Span<char const>> const&>()))
      .WillOnce([&](RestContext&, RestRequest const& request,
                    std::vector<absl::Span<char const>> const&) {
        EXPECT_THAT(request.path(), Eq("/v1/noop"));
        return std::unique_ptr<rest_internal::RestResponse>(
            mock_200_response.release());
      });
  DefaultGoldenRestOnlyRestStub stub(std::move(mock_rest_client), {});
  Status success = stub.Noop(rest_context, Options{}, proto_request);
  EXPECT_THAT(success, IsOk());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1_internal
}  // namespace cloud
}  // namespace google
