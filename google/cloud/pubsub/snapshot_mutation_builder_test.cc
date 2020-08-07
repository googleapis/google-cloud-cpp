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

#include "google/cloud/pubsub/snapshot_mutation_builder.h"
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

TEST(SnapshotMutationBuilder, CreateNoName) {
  auto const subscription = Subscription("test-project", "test-subscription");
  auto const actual =
      SnapshotMutationBuilder().BuildCreateMutation(subscription);
  google::pubsub::v1::CreateSnapshotRequest expected;
  std::string const text = R"pb(
    subscription: "projects/test-project/subscriptions/test-subscription"
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(SnapshotMutationBuilder, CreateWithName) {
  auto const subscription = Subscription("test-project", "test-subscription");
  auto const actual = SnapshotMutationBuilder().BuildCreateMutation(
      subscription, Snapshot("test-project", "test-snapshot"));
  google::pubsub::v1::CreateSnapshotRequest expected;
  std::string const text = R"pb(
    subscription: "projects/test-project/subscriptions/test-subscription"
    name: "projects/test-project/snapshots/test-snapshot"
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(SnapshotMutationBuilder, CreateWithNameAndLabels) {
  auto const subscription = Subscription("test-project", "test-subscription");
  auto const actual =
      SnapshotMutationBuilder()
          .add_label("k0", "v0")
          .add_label("k1", "v1")
          .BuildCreateMutation(subscription,
                               Snapshot("test-project", "test-snapshot"));
  google::pubsub::v1::CreateSnapshotRequest expected;
  std::string const text = R"pb(
    subscription: "projects/test-project/subscriptions/test-subscription"
    name: "projects/test-project/snapshots/test-snapshot"
    labels: { key: "k0" value: "v0" }
    labels: { key: "k1" value: "v1" }
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(SnapshotMutationBuilder, UpdateLabels) {
  auto const actual =
      SnapshotMutationBuilder()
          .add_label("k0", "v0")
          .add_label("k1", "v1")
          .BuildUpdateMutation(Snapshot("test-project", "test-snapshot"));
  google::pubsub::v1::UpdateSnapshotRequest expected;
  std::string const text = R"pb(
    snapshot {
      name: "projects/test-project/snapshots/test-snapshot"
      labels { key: "k0" value: "v0" }
      labels { key: "k1" value: "v1" }
    }
    update_mask { paths: "labels" })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(SnapshotMutationBuilder, UpdateClearLabels) {
  auto const subscription = Subscription("test-project", "test-subscription");
  auto const actual =
      SnapshotMutationBuilder()
          .add_label("k0", "v0")
          .add_label("k1", "v1")
          .clear_labels()
          .add_label("k2", "v2")
          .BuildUpdateMutation(Snapshot("test-project", "test-snapshot"));
  google::pubsub::v1::UpdateSnapshotRequest expected;
  std::string const text = R"pb(
    snapshot {
      name: "projects/test-project/snapshots/test-snapshot"
      labels { key: "k2" value: "v2" }
    }
    update_mask { paths: "labels" })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
