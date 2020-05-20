// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/grpc_client.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

namespace storage_proto = ::google::storage::v1;
using ::google::cloud::testing_util::IsProtoEqual;

TEST(GrpcClientBucketMetadata, BucketAllFields) {
  storage_proto::Bucket input;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
# TODO(#4174) - convert acl() field.
# TODO(#4173) - convert default_object_acl() field.
# TODO(#4165) - convert lifecycle
    time_created: {
      seconds: 1565194924
      nanos: 123456789
    }
    id: "test-bucket-id"
    name: "test-bucket"
    project_number: 123456
    metageneration: 1234567
# TODO(#4169) - convert cors() field.
    location: "test-location"
    storage_class: "test-storage-class"
    etag: "test-etag"
    updated: {
      seconds: 1565194924
      nanos: 123456789
    }
    default_event_based_hold: true
    labels: { key: "test-key-1" value: "test-value-1" }
    labels: { key: "test-key-2" value: "test-value-2" }
# TODO(#4168) - convert website() field.
# TODO(#4167) - convert versioning() field.
# TODO(#4172) - convert logging() field.
# TODO(#4170) - convert owner() field.
# TODO(#4171) - convert encryption() field.
# TODO(#4164) - convert billing() field.
# TODO(#4166) - convert retention_policy() field.
)""",
                                                            &input));

  // To get the dates in RFC-3339 format I used:
  //     date --rfc-3339=seconds --date=@1565194924
  auto expected = BucketMetadataParser::FromString(R"""({
    "timeCreated": "2019-08-07T16:22:04.123456789Z",
    "id": "test-bucket-id",
    "kind": "storage#bucket",
    "name": "test-bucket",
    "projectNumber": 123456,
    "metageneration": 1234567,
    "location": "test-location",
    "storageClass": "test-storage-class",
    "etag": "test-etag",
    "updated": "2019-08-07T16:22:04.123456789Z",
    "defaultEventBasedHold": true,
    "labels": {
        "test-key-1": "test-value-1",
        "test-key-2": "test-value-2"
    }
})""");
  EXPECT_STATUS_OK(expected);

  auto actual = GrpcClient::FromProto(input);
  EXPECT_EQ(actual, *expected);
}

TEST(GrpcClientBucketMetadata, BucketMetadata) {
  auto input = BucketMetadataParser::FromString(R"""({
    "name": "test-bucket"
})""");
  EXPECT_STATUS_OK(input);

  storage_proto::Bucket expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    name: "test-bucket"
# TODO(#4173) - convert the other fields.
)""",
                                                            &expected));

  auto actual = GrpcClient::ToProto(*input);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
