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

#include "google/cloud/storage/internal/grpc_sign_blob_request_parser.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::IsProtoEqual;

TEST(GrpcSignBlobRequestParser, ToProto) {
  google::iam::credentials::v1::SignBlobRequest expected;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "projects/-/serviceAccounts/test-only-sa"
        payload: "test-only-text-to-sign"
        delegates: "test-only-delegate-1"
        delegates: "test-only-delegate-2"
      )pb",
      &expected));

  auto b64encoded = Base64Encode("test-only-text-to-sign");

  auto request =
      SignBlobRequest("test-only-sa", std::move(b64encoded),
                      {"test-only-delegate-1", "test-only-delegate-2"});
  auto const actual = ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcSignBlobRequestParser, FromProto) {
  google::iam::credentials::v1::SignBlobResponse input;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        key_id: "test-only-key-id" signed_blob: "test-only-signed-blob"
      )pb",
      &input));
  auto response = FromProto(input);
  EXPECT_EQ(response.key_id, "test-only-key-id");
  EXPECT_EQ(response.signed_blob, Base64Encode("test-only-signed-blob"));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
