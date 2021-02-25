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

#include "google/cloud/pubsub/topic_builder.h"
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

TEST(TopicBuilder, CreateTopicOnly) {
  auto const actual =
      TopicBuilder(Topic("test-project", "test-topic")).BuildCreateRequest();
  google::pubsub::v1::Topic expected;
  std::string const text = R"pb(
    name: "projects/test-project/topics/test-topic"
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(TopicBuilder, AddLabel) {
  auto const actual = TopicBuilder(Topic("test-project", "test-topic"))
                          .add_label("key0", "label0")
                          .add_label("key1", "label1")
                          .BuildUpdateRequest();
  google::pubsub::v1::UpdateTopicRequest expected;
  std::string const text = R"pb(
    topic {
      name: "projects/test-project/topics/test-topic"
      labels: { key: "key1" value: "label1" }
      labels: { key: "key0" value: "label0" }
    }
    update_mask { paths: "labels" })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(TopicBuilder, ClearLabel) {
  auto const actual = TopicBuilder(Topic("test-project", "test-topic"))
                          .add_label("key0", "label0")
                          .clear_labels()
                          .add_label("key1", "label1")
                          .BuildUpdateRequest();
  google::pubsub::v1::UpdateTopicRequest expected;
  std::string const text = R"pb(
    topic {
      name: "projects/test-project/topics/test-topic"
      labels: { key: "key1" value: "label1" }
    }
    update_mask { paths: "labels" })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(TopicBuilder, AddAllowedPersistenceRegion) {
  auto const actual = TopicBuilder(Topic("test-project", "test-topic"))
                          .add_allowed_persistence_region("us-central1")
                          .add_allowed_persistence_region("us-west1")
                          .BuildUpdateRequest();
  google::pubsub::v1::UpdateTopicRequest expected;
  std::string const text = R"pb(
    topic {
      name: "projects/test-project/topics/test-topic"
      message_storage_policy {
        allowed_persistence_regions: "us-central1"
        allowed_persistence_regions: "us-west1"
      }
    }
    update_mask { paths: "message_storage_policy" })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(TopicBuilder, ClearAllowedPersistenceRegions) {
  auto const actual = TopicBuilder(Topic("test-project", "test-topic"))
                          .add_allowed_persistence_region("us-central1")
                          .clear_allowed_persistence_regions()
                          .add_allowed_persistence_region("us-west1")
                          .BuildUpdateRequest();
  google::pubsub::v1::UpdateTopicRequest expected;
  std::string const text = R"pb(
    topic {
      name: "projects/test-project/topics/test-topic"
      message_storage_policy { allowed_persistence_regions: "us-west1" }
    }
    update_mask { paths: "message_storage_policy" })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(TopicBuilder, SetKmsKeyName) {
  auto const actual = TopicBuilder(Topic("test-project", "test-topic"))
                          .set_kms_key_name("projects/.../test-only-string")
                          .BuildUpdateRequest();
  google::pubsub::v1::UpdateTopicRequest expected;
  std::string const text = R"pb(
    topic {
      name: "projects/test-project/topics/test-topic"
      kms_key_name: "projects/.../test-only-string"
    }
    update_mask { paths: "kms_key_name" })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(TopicBuilder, SetSchema) {
  auto const actual = TopicBuilder(Topic("test-project", "test-topic"))
                          .experimental_set_schema(pubsub_experimental::Schema(
                              "test-project", "test-schema"))
                          .BuildUpdateRequest();
  google::pubsub::v1::UpdateTopicRequest expected;
  std::string const text = R"pb(
    topic {
      name: "projects/test-project/topics/test-topic"
      schema_settings { schema: "projects/test-project/schemas/test-schema" }
    }
    update_mask { paths: "schema_settings.schema" })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(TopicBuilder, SetEncoding) {
  auto const actual = TopicBuilder(Topic("test-project", "test-topic"))
                          .experimental_set_encoding(google::pubsub::v1::JSON)
                          .BuildUpdateRequest();
  google::pubsub::v1::UpdateTopicRequest expected;
  std::string const text = R"pb(
    topic {
      name: "projects/test-project/topics/test-topic"
      schema_settings { encoding: JSON }
    }
    update_mask { paths: "schema_settings.encoding" })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(TopicBuilder, MultipleChanges) {
  auto builder = TopicBuilder(Topic("test-project", "test-topic"))
                     .add_label("key0", "label0")
                     .add_label("key1", "label1")
                     .add_allowed_persistence_region("us-central1")
                     .add_allowed_persistence_region("us-west1")
                     .set_kms_key_name("projects/.../test-only-string");
  auto const actual = std::move(builder).BuildUpdateRequest();
  google::pubsub::v1::UpdateTopicRequest expected;
  std::string const text = R"pb(
    topic {
      name: "projects/test-project/topics/test-topic"
      labels: { key: "key1" value: "label1" }
      labels: { key: "key0" value: "label0" }
      message_storage_policy {
        allowed_persistence_regions: "us-central1"
        allowed_persistence_regions: "us-west1"
      }
      kms_key_name: "projects/.../test-only-string"
    }
    update_mask {
      paths: "kms_key_name"
      paths: "labels"
      paths: "message_storage_policy"
    })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
