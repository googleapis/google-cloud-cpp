// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/update_instance_request_builder.h"
#include <google/protobuf/util/field_mask_util.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

TEST(UpdateInstanceRequestBuilder, Constructors) {
  std::string expected_name = "projects/test-project/instances/test-instance";
  UpdateInstanceRequestBuilder builder(expected_name);
  auto req = builder.Build();
  EXPECT_EQ(expected_name, req.instance().name());

  Instance in("test-project", "test-instance");
  builder = UpdateInstanceRequestBuilder(in);
  req = builder.Build();
  EXPECT_EQ(expected_name, req.instance().name());

  google::spanner::admin::instance::v1::Instance instance;
  instance.set_name(expected_name);
  builder = UpdateInstanceRequestBuilder(instance);
  req = builder.Build();
  EXPECT_EQ(expected_name, req.instance().name());

  req = UpdateInstanceRequestBuilder().SetName(expected_name).Build();
  EXPECT_EQ(expected_name, req.instance().name());
}

TEST(UpdateInstanceRequestBuilder, AddLabels) {
  std::string expected_name = "projects/test-project/instances/test-instance";
  std::string expected_display_name =
      "projects/test-project/instances/test-display-name";
  google::spanner::admin::instance::v1::Instance instance;
  instance.set_name(expected_name);
  instance.set_display_name("projects/test-project/insance/old-display-name");
  instance.set_node_count(1);
  instance.mutable_labels()->insert({"key", "value"});
  auto builder = UpdateInstanceRequestBuilder(instance);
  auto req = builder.SetNodeCount(2)
                 .SetDisplayName(expected_display_name)
                 .AddLabels({{"newkey", "newvalue"}})
                 .Build();
  EXPECT_EQ(expected_name, req.instance().name());
  EXPECT_EQ(expected_display_name, req.instance().display_name());
  EXPECT_EQ(2, req.instance().node_count());
  EXPECT_EQ(2, req.instance().labels_size());
  EXPECT_EQ("newvalue", req.instance().labels().at("newkey"));
  EXPECT_TRUE(google::protobuf::util::FieldMaskUtil::IsPathInFieldMask(
      "display_name", req.field_mask()));
  EXPECT_TRUE(google::protobuf::util::FieldMaskUtil::IsPathInFieldMask(
      "node_count", req.field_mask()));
  EXPECT_TRUE(google::protobuf::util::FieldMaskUtil::IsPathInFieldMask(
      "labels", req.field_mask()));
}

TEST(UpdateInstanceRequestBuilder, AddLabelsRvalueReference) {
  std::string expected_name = "projects/test-project/instances/test-instance";
  std::string expected_display_name =
      "projects/test-project/instances/test-display-name";
  google::spanner::admin::instance::v1::Instance instance;
  instance.set_name(expected_name);
  instance.set_display_name("projects/test-project/insance/old-display-name");
  instance.set_node_count(1);
  instance.mutable_labels()->insert({"key", "value"});
  auto req = UpdateInstanceRequestBuilder(instance)
                 .SetNodeCount(2)
                 .SetDisplayName(expected_display_name)
                 .AddLabels({{"newkey", "newvalue"}})
                 .Build();
  EXPECT_EQ(expected_name, req.instance().name());
  EXPECT_EQ(expected_display_name, req.instance().display_name());
  EXPECT_EQ(2, req.instance().node_count());
  EXPECT_EQ(2, req.instance().labels_size());
  EXPECT_EQ("newvalue", req.instance().labels().at("newkey"));
  EXPECT_TRUE(google::protobuf::util::FieldMaskUtil::IsPathInFieldMask(
      "display_name", req.field_mask()));
  EXPECT_TRUE(google::protobuf::util::FieldMaskUtil::IsPathInFieldMask(
      "node_count", req.field_mask()));
  EXPECT_TRUE(google::protobuf::util::FieldMaskUtil::IsPathInFieldMask(
      "labels", req.field_mask()));
}

TEST(UpdateInstanceRequestBuilder, SetLabels) {
  std::string expected_name = "projects/test-project/instances/test-instance";
  std::string expected_display_name =
      "projects/test-project/instances/test-display-name";
  google::spanner::admin::instance::v1::Instance instance;
  instance.set_name(expected_name);
  instance.set_display_name("projects/test-project/insance/old-display-name");
  instance.set_node_count(1);
  instance.mutable_labels()->insert({"key", "value"});
  auto builder = UpdateInstanceRequestBuilder(instance);
  auto req = builder.SetNodeCount(2)
                 .SetDisplayName(expected_display_name)
                 .SetLabels({{"newkey", "newvalue"}})
                 .Build();
  EXPECT_EQ(expected_name, req.instance().name());
  EXPECT_EQ(expected_display_name, req.instance().display_name());
  EXPECT_EQ(2, req.instance().node_count());
  EXPECT_EQ(1, req.instance().labels_size());
  EXPECT_EQ("newvalue", req.instance().labels().at("newkey"));
  EXPECT_TRUE(google::protobuf::util::FieldMaskUtil::IsPathInFieldMask(
      "display_name", req.field_mask()));
  EXPECT_TRUE(google::protobuf::util::FieldMaskUtil::IsPathInFieldMask(
      "node_count", req.field_mask()));
  EXPECT_TRUE(google::protobuf::util::FieldMaskUtil::IsPathInFieldMask(
      "labels", req.field_mask()));
}

TEST(UpdateInstanceRequestBuilder, SetLabelsRvalueReference) {
  std::string expected_name = "projects/test-project/instances/test-instance";
  std::string expected_display_name =
      "projects/test-project/instances/test-display-name";
  google::spanner::admin::instance::v1::Instance instance;
  instance.set_name(expected_name);
  instance.set_display_name("projects/test-project/insance/old-display-name");
  instance.set_node_count(1);
  instance.mutable_labels()->insert({"key", "value"});
  auto req = UpdateInstanceRequestBuilder(instance)
                 .SetNodeCount(2)
                 .SetDisplayName(expected_display_name)
                 .SetLabels({{"newkey", "newvalue"}})
                 .Build();
  EXPECT_EQ(expected_name, req.instance().name());
  EXPECT_EQ(expected_display_name, req.instance().display_name());
  EXPECT_EQ(2, req.instance().node_count());
  EXPECT_EQ(1, req.instance().labels_size());
  EXPECT_EQ("newvalue", req.instance().labels().at("newkey"));
  EXPECT_TRUE(google::protobuf::util::FieldMaskUtil::IsPathInFieldMask(
      "display_name", req.field_mask()));
  EXPECT_TRUE(google::protobuf::util::FieldMaskUtil::IsPathInFieldMask(
      "node_count", req.field_mask()));
  EXPECT_TRUE(google::protobuf::util::FieldMaskUtil::IsPathInFieldMask(
      "labels", req.field_mask()));
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
