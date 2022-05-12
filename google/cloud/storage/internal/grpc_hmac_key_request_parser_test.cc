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

#include "google/cloud/storage/internal/grpc_hmac_key_request_parser.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

namespace v2 = ::google::storage::v2;
using ::google::cloud::testing_util::IsProtoEqual;

TEST(GrpcBucketRequestParser, GetHmacKeyRequestAllOptions) {
  v2::GetHmacKeyRequest expected;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        access_id: "test-access-id"
        project: "projects/test-project-id"
        common_request_params: { user_project: "test-user-project" }
      )pb",
      &expected));

  GetHmacKeyRequest req("test-project-id", "test-access-id");
  req.set_multiple_options(UserProject("test-user-project"));

  auto const actual = GrpcHmacKeyRequestParser::ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcBucketRequestParser, DeleteHmacKeyRequestAllOptions) {
  v2::DeleteHmacKeyRequest expected;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        access_id: "test-access-id"
        project: "projects/test-project-id"
        common_request_params: { user_project: "test-user-project" }
      )pb",
      &expected));

  DeleteHmacKeyRequest req("test-project-id", "test-access-id");
  req.set_multiple_options(UserProject("test-user-project"));

  auto const actual = GrpcHmacKeyRequestParser::ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
