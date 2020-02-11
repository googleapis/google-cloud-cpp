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

#include "google/cloud/pubsub/create_topic_builder.h"
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gmock/gmock.h>
#include <sstream>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::protobuf::TextFormat;

MATCHER_P(IsProtoEqual, value, "Checks whether protos are equal") {
  std::string delta;
  google::protobuf::util::MessageDifferencer differencer;
  differencer.ReportDifferencesToString(&delta);
  auto const result = differencer.Compare(arg, value);
  *result_listener << "\n" << delta;
  return result;
}

TEST(Topic, TopicOnly) {
  auto const actual =
      CreateTopicBuilder(Topic("test-project", "test-topic")).as_proto();
  google::pubsub::v1::Topic expected;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        name: "projects/test-project/topics/test-topic"
      )pb",
      &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(Topic, AddLabel) {
  auto const actual = CreateTopicBuilder(Topic("test-project", "test-topic"))
                          .add_label("key0", "label0")
                          .add_label("key1", "label1")
                          .as_proto();
  google::pubsub::v1::Topic expected;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        name: "projects/test-project/topics/test-topic"
        labels: { key: "key1" value: "label1" }
        labels: { key: "key0" value: "label0" }
      )pb",
      &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(Topic, ClearLabel) {
  auto const actual = CreateTopicBuilder(Topic("test-project", "test-topic"))
                          .add_label("key0", "label0")
                          .clear_labels()
                          .add_label("key1", "label1")
                          .as_proto();
  google::pubsub::v1::Topic expected;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        name: "projects/test-project/topics/test-topic"
        labels: { key: "key1" value: "label1" }
      )pb",
      &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(Topic, AddAllowedPersistenceRegion) {
  auto const actual = CreateTopicBuilder(Topic("test-project", "test-topic"))
                          .add_allowed_persistence_region("us-central1")
                          .add_allowed_persistence_region("us-west1")
                          .as_proto();
  google::pubsub::v1::Topic expected;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        name: "projects/test-project/topics/test-topic"
        message_storage_policy {
          allowed_persistence_regions: "us-central1"
          allowed_persistence_regions: "us-west1"
        }
      )pb",
      &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(Topic, ClearAllowedPersistenceRegions) {
  auto const actual = CreateTopicBuilder(Topic("test-project", "test-topic"))
                          .add_allowed_persistence_region("us-central1")
                          .clear_allowed_persistence_regions()
                          .add_allowed_persistence_region("us-west1")
                          .as_proto();
  google::pubsub::v1::Topic expected;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        name: "projects/test-project/topics/test-topic"
        message_storage_policy { allowed_persistence_regions: "us-west1" }
      )pb",
      &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(Topic, SetKmsKeyName) {
  auto const actual = CreateTopicBuilder(Topic("test-project", "test-topic"))
                          .set_kms_key_name("projects/.../test-only-string")
                          .as_proto();
  google::pubsub::v1::Topic expected;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        name: "projects/test-project/topics/test-topic"
        kms_key_name: "projects/.../test-only-string"
      )pb",
      &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
