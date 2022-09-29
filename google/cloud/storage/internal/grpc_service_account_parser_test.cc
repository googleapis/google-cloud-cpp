// Copyright 2022 Google LLC
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

#include "google/cloud/storage/internal/grpc_service_account_parser.h"
#include "google/cloud/storage/internal/service_account_parser.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace storage_proto = ::google::storage::v2;
using ::google::cloud::testing_util::IsProtoEqual;

TEST(GrpcServiceAccountParser, FromProtoServiceAccount) {
  storage_proto::ServiceAccount response;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(email_address: "test-only@example.com")pb", &response));
  auto const expected =
      storage::internal::ServiceAccountParser::FromString(R"""({
    "email_address": "test-only@example.com",
    "kind": "storage#serviceAccount"
  })""");
  ASSERT_STATUS_OK(expected);

  auto const actual = FromProto(response);
  EXPECT_EQ(*expected, actual);
}

TEST(GrpcServiceAccountParser, ToProtoGetServiceAccount) {
  storage_proto::GetServiceAccountRequest expected;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(project: "projects/test-only-project-id")pb", &expected));
  storage::internal::GetProjectServiceAccountRequest request(
      "test-only-project-id");
  request.set_multiple_options(storage::UserProject("test-only-user-project"));
  auto const actual = ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
