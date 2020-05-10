// Copyright 2018 Google LLC
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

#include "google/cloud/bigtable/instance_update_config.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace btadmin = ::google::bigtable::admin::v2;
namespace bigtable = google::cloud::bigtable;

TEST(InstanceUpdateConfigTest, Constructor) {
  std::string instance_text = R"(
      name: 'projects/my-project/instances/test-instance'
      display_name: 'foo bar'
      state: READY
      type: PRODUCTION
      labels: {
        key: 'foo1'
        value: 'bar1'
      }
      labels: {
        key: 'foo2'
        value: 'bar2'
      }
  )";

  btadmin::Instance instance;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(instance_text, &instance));
  bigtable::InstanceUpdateConfig config(std::move(instance));
  auto const& proto = config.as_proto();
  EXPECT_EQ("projects/my-project/instances/test-instance",
            proto.instance().name());
  EXPECT_EQ("foo bar", proto.instance().display_name());
  ASSERT_EQ(2, proto.instance().labels_size());
}

TEST(InstanceUpdateConfigTest, UpdateMask) {
  std::string instance_text = R"(
      name: 'projects/my-project/instances/test-instance'
      display_name: 'foo bar'
      state: READY
      type: PRODUCTION
      labels: {
        key: 'foo1'
        value: 'bar1'
      }
      labels: {
        key: 'foo2'
        value: 'bar2'
      }
  )";

  btadmin::Instance instance;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(instance_text, &instance));
  bigtable::InstanceUpdateConfig config(std::move(instance));
  config.set_display_name("foo1");
  auto proto = config.as_proto();
  EXPECT_EQ("projects/my-project/instances/test-instance",
            proto.instance().name());
  EXPECT_EQ("foo1", proto.instance().display_name());
  ASSERT_EQ(1, proto.update_mask().paths_size());

  config.set_display_name("foo2");
  proto = config.as_proto();

  EXPECT_EQ("foo2", proto.instance().display_name());
  ASSERT_EQ(1, proto.update_mask().paths_size());
}

TEST(InstanceUpdateConfigTest, SetLabels) {
  std::string instance_text = R"(
      name: 'projects/my-project/instances/test-instance'
      display_name: 'foo bar'
      state: READY
      type: PRODUCTION
  )";

  btadmin::Instance instance;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(instance_text, &instance));
  bigtable::InstanceUpdateConfig config(std::move(instance));

  config.insert_label("foo", "bar").emplace_label("baz", "qux");

  auto proto = config.as_proto();
  EXPECT_EQ("projects/my-project/instances/test-instance",
            proto.instance().name());
  EXPECT_EQ("foo bar", proto.instance().display_name());
  ASSERT_EQ(2, proto.instance().labels_size());
  EXPECT_EQ("bar", proto.instance().labels().at("foo"));
  EXPECT_EQ("qux", proto.instance().labels().at("baz"));
}
