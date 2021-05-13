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

#include "google/cloud/spanner/create_instance_request_builder.h"
#include "google/cloud/spanner/instance.h"
#include <google/protobuf/util/field_mask_util.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

TEST(CreateInstanceRequestBuilder, DefaultValues) {
  std::string expected_name = "projects/test-project/instances/test-instance";
  std::string expected_config =
      "projects/test-project/instanceConfigs/test-config";
  std::string expected_display_name = "test-instance";
  CreateInstanceRequestBuilder builder(
      Instance("test-project", "test-instance"), expected_config);
  auto req = builder.Build();
  EXPECT_EQ("projects/test-project", req.parent());
  EXPECT_EQ("test-instance", req.instance_id());
  EXPECT_EQ(expected_name, req.instance().name());
  EXPECT_EQ(expected_config, req.instance().config());
  EXPECT_EQ(1, req.instance().node_count());
  EXPECT_EQ(0, req.instance().labels_size());
  EXPECT_EQ(expected_display_name, req.instance().display_name());
}

TEST(CreateInstanceRequestBuilder, RvalueReference) {
  std::string expected_name = "projects/test-project/instances/test-instance";
  std::string expected_config =
      "projects/test-project/instanceConfigs/test-config";
  std::string expected_display_name = "test-display-name";
  Instance in("test-project", "test-instance");

  auto req = CreateInstanceRequestBuilder(in, expected_config)
                 .SetDisplayName(expected_display_name)
                 .SetNodeCount(1)
                 .SetLabels({{"key", "value"}})
                 .Build();
  EXPECT_EQ("projects/test-project", req.parent());
  EXPECT_EQ("test-instance", req.instance_id());
  EXPECT_EQ(expected_name, req.instance().name());
  EXPECT_EQ(expected_config, req.instance().config());
  EXPECT_EQ(1, req.instance().node_count());
  EXPECT_EQ(1, req.instance().labels_size());
  EXPECT_EQ("value", req.instance().labels().at("key"));
  EXPECT_EQ(expected_display_name, req.instance().display_name());
}

TEST(CreateInstanceRequestBuilder, Lvalue) {
  std::string expected_name = "projects/test-project/instances/test-instance";
  std::string expected_config =
      "projects/test-project/instanceConfigs/test-config";
  std::string expected_display_name = "test-display-name";
  Instance in("test-project", "test-instance");

  auto builder = CreateInstanceRequestBuilder(in, expected_config);
  auto req = builder.SetDisplayName(expected_display_name)
                 .SetProcessingUnits(500)
                 .SetLabels({{"key", "value"}})
                 .Build();
  EXPECT_EQ("projects/test-project", req.parent());
  EXPECT_EQ("test-instance", req.instance_id());
  EXPECT_EQ(expected_name, req.instance().name());
  EXPECT_EQ(expected_config, req.instance().config());
  EXPECT_EQ(500, req.instance().processing_units());
  EXPECT_EQ(1, req.instance().labels_size());
  EXPECT_EQ("value", req.instance().labels().at("key"));
  EXPECT_EQ(expected_display_name, req.instance().display_name());
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
