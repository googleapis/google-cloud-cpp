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

#include "google/cloud/storage/internal/grpc_hmac_key_metadata_parser.h"
#include "google/cloud/storage/internal/hmac_key_metadata_parser.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

namespace storage_proto = ::google::storage::v2;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::protobuf::TextFormat;

TEST(GrpcHmacKeyMetadataParser, Roundtrip) {
  storage_proto::HmacKeyMetadata input;
  // To get the dates in RFC-3339 format I used:
  //     date --rfc-3339=seconds --date=@1652099696  # Create
  //     date --rfc-3339=seconds --date=@1652186096  # Update
  auto constexpr kProtoText = R"pb(
    id: "test-id"
    access_id: "test-access-id"
    project: "projects/test-project"
    service_account_email: "test-account@test-project.test"
    state: "INACTIVE"
    create_time { seconds: 1652099696 nanos: 789000000 }
    update_time { seconds: 1652186096 nanos: 789000000 })pb";
  EXPECT_TRUE(TextFormat::ParseFromString(kProtoText, &input));

  auto const actual = GrpcHmacKeyMetadataParser::FromProto(input);
  EXPECT_EQ(actual.id(), "test-id");
  EXPECT_EQ(actual.access_id(), "test-access-id");
  EXPECT_EQ(actual.state(), "INACTIVE");

  auto const output = GrpcHmacKeyMetadataParser::ToProto(actual);
  EXPECT_THAT(output, IsProtoEqual(input));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
