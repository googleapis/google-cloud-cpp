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

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

UpdateInstanceRequestBuilder& UpdateInstanceRequestBuilder::SetDisplayName(
    std::string display_name) {
  if (!google::protobuf::util::FieldMaskUtil::IsPathInFieldMask(
          "display_name", request_.field_mask())) {
    request_.mutable_field_mask()->add_paths("display_name");
  }
  request_.mutable_instance()->set_display_name(std::move(display_name));
  return *this;
}

UpdateInstanceRequestBuilder& UpdateInstanceRequestBuilder::SetNodeCount(
    int node_count) {
  if (!google::protobuf::util::FieldMaskUtil::IsPathInFieldMask(
          "node_count", request_.field_mask())) {
    request_.mutable_field_mask()->add_paths("node_count");
  }
  request_.mutable_instance()->set_node_count(node_count);
  return *this;
}

UpdateInstanceRequestBuilder& UpdateInstanceRequestBuilder::AddLabels(
    std::map<std::string, std::string> const& labels) {
  if (!google::protobuf::util::FieldMaskUtil::IsPathInFieldMask(
          "labels", request_.field_mask())) {
    request_.mutable_field_mask()->add_paths("labels");
  }
  for (auto const& pair : labels) {
    request_.mutable_instance()->mutable_labels()->insert(
        {pair.first, pair.second});
  }
  return *this;
}

UpdateInstanceRequestBuilder& UpdateInstanceRequestBuilder::SetLabels(
    std::map<std::string, std::string> const& labels) {
  request_.mutable_instance()->clear_labels();
  return AddLabels(labels);
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
