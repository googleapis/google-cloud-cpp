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
#include "google/cloud/testing_util/is_proto_equal.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <sstream>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::testing_util::IsProtoEqual;
using ::google::protobuf::TextFormat;

TEST(Topic, TopicOnly) {
  auto const actual =
      CreateTopicBuilder(Topic("test-project", "test-topic")).as_proto();
  google::pubsub::v1::Topic expected;
  std::string const text = R"pb(
    name: "projects/test-project/topics/test-topic"
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(Topic, AddLabel) {
  auto const actual = CreateTopicBuilder(Topic("test-project", "test-topic"))
                          .add_label("key0", "label0")
                          .add_label("key1", "label1")
                          .as_proto();
  google::pubsub::v1::Topic expected;
  std::string const text = R"pb(
    name: "projects/test-project/topics/test-topic"
    labels: { key: "key1" value: "label1" }
    labels: { key: "key0" value: "label0" }
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(Topic, ClearLabel) {
  auto const actual = CreateTopicBuilder(Topic("test-project", "test-topic"))
                          .add_label("key0", "label0")
                          .clear_labels()
                          .add_label("key1", "label1")
                          .as_proto();
  google::pubsub::v1::Topic expected;
  std::string const text = R"pb(
    name: "projects/test-project/topics/test-topic"
    labels: { key: "key1" value: "label1" }
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(Topic, AddAllowedPersistenceRegion) {
  auto const actual = CreateTopicBuilder(Topic("test-project", "test-topic"))
                          .add_allowed_persistence_region("us-central1")
                          .add_allowed_persistence_region("us-west1")
                          .as_proto();
  google::pubsub::v1::Topic expected;
  std::string const text = R"pb(
    name: "projects/test-project/topics/test-topic"
    message_storage_policy {
      allowed_persistence_regions: "us-central1"
      allowed_persistence_regions: "us-west1"
    }
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(Topic, ClearAllowedPersistenceRegions) {
  auto const actual = CreateTopicBuilder(Topic("test-project", "test-topic"))
                          .add_allowed_persistence_region("us-central1")
                          .clear_allowed_persistence_regions()
                          .add_allowed_persistence_region("us-west1")
                          .as_proto();
  google::pubsub::v1::Topic expected;
  std::string const text = R"pb(
    name: "projects/test-project/topics/test-topic"
    message_storage_policy { allowed_persistence_regions: "us-west1" }
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(Topic, SetKmsKeyName) {
  auto const actual = CreateTopicBuilder(Topic("test-project", "test-topic"))
                          .set_kms_key_name("projects/.../test-only-string")
                          .as_proto();
  google::pubsub::v1::Topic expected;
  std::string const text = R"pb(
    name: "projects/test-project/topics/test-topic"
    kms_key_name: "projects/.../test-only-string"
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(Topic, MoveProto) {
  auto builder = CreateTopicBuilder(Topic("test-project", "test-topic"))
                     .add_label("key0", "label0")
                     .add_label("key1", "label1")
                     .add_allowed_persistence_region("us-central1")
                     .add_allowed_persistence_region("us-west1")
                     .set_kms_key_name("projects/.../test-only-string");
  auto const actual = std::move(builder).as_proto();
  google::pubsub::v1::Topic expected;
  std::string const text = R"pb(
    name: "projects/test-project/topics/test-topic"
    labels: { key: "key1" value: "label1" }
    labels: { key: "key0" value: "label0" }
    message_storage_policy {
      allowed_persistence_regions: "us-central1"
      allowed_persistence_regions: "us-west1"
    }
    kms_key_name: "projects/.../test-only-string"
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
