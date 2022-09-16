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

#include "google/cloud/storage/internal/grpc_notification_metadata_parser.h"
#include "google/cloud/storage/internal/notification_metadata_parser.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace storage_proto = ::google::storage::v2;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::protobuf::TextFormat;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(GrpcNotificationMetadataParser, Roundtrip) {
  storage_proto::Notification input;
  auto constexpr kProtoText = R"pb(
    name: "projects/_/buckets/test-bucket-name/notificationConfigs/test-notification-id"
    topic: "//pubsub.googleapis.com/projects/test-project-id/topics/test-topic-id"
    etag: "test-etag"
    event_types: "OBJECT_FINALIZE"
    event_types: "OBJECT_METADATA_UPDATE"
    event_types: "OBJECT_DELETE"
    event_types: "OBJECT_ARCHIVE"
    custom_attributes { key: "test-key-0" value: "test-value-0" }
    custom_attributes { key: "test-key-1" value: "test-value-1" }
    object_name_prefix: "test-object-name-prefix/"
    payload_format: "JSON_API_V1"
  )pb";
  EXPECT_TRUE(TextFormat::ParseFromString(kProtoText, &input));

  auto const actual = FromProto(input);
  EXPECT_EQ(actual.id(), "test-notification-id");
  EXPECT_EQ(actual.topic(), "projects/test-project-id/topics/test-topic-id");
  EXPECT_EQ(actual.etag(), "test-etag");
  EXPECT_THAT(actual.event_types(),
              UnorderedElementsAre("OBJECT_FINALIZE", "OBJECT_METADATA_UPDATE",
                                   "OBJECT_DELETE", "OBJECT_ARCHIVE"));
  EXPECT_THAT(actual.custom_attributes(),
              UnorderedElementsAre(Pair("test-key-0", "test-value-0"),
                                   Pair("test-key-1", "test-value-1")));
  EXPECT_EQ(actual.object_name_prefix(), "test-object-name-prefix/");
  EXPECT_EQ(actual.payload_format(), "JSON_API_V1");

  auto const output = ToProto(actual, "test-bucket-name");
  EXPECT_THAT(output, IsProtoEqual(input));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
