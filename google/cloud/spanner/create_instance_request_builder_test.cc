// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/create_instance_request_builder.h"
#include "google/cloud/spanner/instance.h"
#include <google/protobuf/util/field_mask_util.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

TEST(CreateInstanceRequestBuilder, Equality) {
  Instance in(Project("test-project"), "test-instance");
  CreateInstanceRequestBuilder b1(in, "c1");
  CreateInstanceRequestBuilder b2(in, "c2");
  EXPECT_NE(b1, b2);

  b2 = b1;
  EXPECT_EQ(b1, b2);
}

TEST(CreateInstanceRequestBuilder, DefaultValues) {
  Instance in(Project("test-project"), "test-instance");
  std::string expected_config =
      in.project().FullName() + "/instanceConfigs/test-config";
  std::string const& expected_display_name = in.instance_id();
  CreateInstanceRequestBuilder builder(in, expected_config);
  auto req = builder.Build();
  EXPECT_EQ(in.project().FullName(), req.parent());
  EXPECT_EQ(in.instance_id(), req.instance_id());
  EXPECT_EQ(in.FullName(), req.instance().name());
  EXPECT_EQ(expected_config, req.instance().config());
  EXPECT_EQ(1, req.instance().node_count());
  EXPECT_EQ(0, req.instance().labels_size());
  EXPECT_EQ(expected_display_name, req.instance().display_name());
}

TEST(CreateInstanceRequestBuilder, RvalueReference) {
  Instance in(Project("test-project"), "test-instance");
  std::string expected_config =
      in.project().FullName() + "/instanceConfigs/test-config";
  std::string expected_display_name = "test-display-name";

  auto req = CreateInstanceRequestBuilder(in, expected_config)
                 .SetDisplayName(expected_display_name)
                 .SetNodeCount(1)
                 .SetLabels({{"key", "value"}})
                 .Build();
  EXPECT_EQ(in.project().FullName(), req.parent());
  EXPECT_EQ(in.instance_id(), req.instance_id());
  EXPECT_EQ(in.FullName(), req.instance().name());
  EXPECT_EQ(expected_config, req.instance().config());
  EXPECT_EQ(1, req.instance().node_count());
  EXPECT_EQ(1, req.instance().labels_size());
  EXPECT_EQ("value", req.instance().labels().at("key"));
  EXPECT_EQ(expected_display_name, req.instance().display_name());
}

TEST(CreateInstanceRequestBuilder, Lvalue) {
  Instance in(Project("test-project"), "test-instance");
  std::string expected_config =
      in.project().FullName() + "/instanceConfigs/test-config";
  std::string expected_display_name = "test-display-name";

  auto builder = CreateInstanceRequestBuilder(in, expected_config);
  auto req = builder.SetDisplayName(expected_display_name)
                 .SetProcessingUnits(500)
                 .SetLabels({{"key", "value"}})
                 .Build();
  EXPECT_EQ(in.project().FullName(), req.parent());
  EXPECT_EQ(in.instance_id(), req.instance_id());
  EXPECT_EQ(in.FullName(), req.instance().name());
  EXPECT_EQ(expected_config, req.instance().config());
  EXPECT_EQ(500, req.instance().processing_units());
  EXPECT_EQ(1, req.instance().labels_size());
  EXPECT_EQ("value", req.instance().labels().at("key"));
  EXPECT_EQ(expected_display_name, req.instance().display_name());
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
