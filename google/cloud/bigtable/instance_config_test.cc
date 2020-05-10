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

#include "google/cloud/bigtable/instance_config.h"
#include <gmock/gmock.h>

namespace bigtable = google::cloud::bigtable;

TEST(InstanceConfigTest, Constructor) {
  bigtable::InstanceConfig config(
      "my-instance", "pretty name",
      {{"my-cluster", {"somewhere", 7, bigtable::ClusterConfig::SSD}}});
  auto const& proto = config.as_proto();
  EXPECT_EQ("my-instance", proto.instance_id());
  EXPECT_EQ("pretty name", proto.instance().display_name());
  ASSERT_EQ(1, proto.clusters_size());
  auto cluster = proto.clusters().at("my-cluster");
  EXPECT_EQ("somewhere", cluster.location());
  EXPECT_EQ(7, cluster.serve_nodes());
  EXPECT_EQ(bigtable::ClusterConfig::SSD, cluster.default_storage_type());
}

TEST(InstanceConfigTest, ConstructorManyClusters) {
  bigtable::InstanceConfig config(
      "my-instance", "pretty name",
      {
          {"cluster-1", {"somewhere", 7, bigtable::ClusterConfig::SSD}},
          {"cluster-2", {"elsewhere", 7, bigtable::ClusterConfig::HDD}},
          {"cluster-3", {"nowhere", 17, bigtable::ClusterConfig::HDD}},
      });
  auto const& proto = config.as_proto();
  EXPECT_EQ("my-instance", proto.instance_id());
  EXPECT_EQ("pretty name", proto.instance().display_name());
  ASSERT_EQ(3, proto.clusters_size());

  auto c1 = proto.clusters().at("cluster-1");
  EXPECT_EQ("somewhere", c1.location());
  EXPECT_EQ(7, c1.serve_nodes());
  EXPECT_EQ(bigtable::ClusterConfig::SSD, c1.default_storage_type());

  auto c3 = proto.clusters().at("cluster-3");
  EXPECT_EQ("nowhere", c3.location());
  EXPECT_EQ(17, c3.serve_nodes());
  EXPECT_EQ(bigtable::ClusterConfig::HDD, c3.default_storage_type());
}

TEST(InstanceConfigTest, SetLabels) {
  bigtable::InstanceConfig config(
      "my-instance", "pretty name",
      {
          {"cluster-1", {"somewhere", 7, bigtable::ClusterConfig::SSD}},
      });

  config.insert_label("foo", "bar").emplace_label("baz", "qux");

  auto proto = config.as_proto();
  EXPECT_EQ("my-instance", proto.instance_id());
  EXPECT_EQ("pretty name", proto.instance().display_name());
  ASSERT_EQ(1, proto.clusters_size());

  ASSERT_EQ(2, proto.instance().labels_size());
  EXPECT_EQ("bar", proto.instance().labels().at("foo"));
  EXPECT_EQ("qux", proto.instance().labels().at("baz"));
}
