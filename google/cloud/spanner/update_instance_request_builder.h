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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_UPDATE_INSTANCE_REQUEST_BUILDER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_UPDATE_INSTANCE_REQUEST_BUILDER_H

#include "google/cloud/spanner/instance.h"
#include "google/cloud/spanner/version.h"
#include <google/protobuf/util/field_mask_util.h>
#include <google/spanner/admin/instance/v1/spanner_instance_admin.pb.h>
#include <map>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * UpdateInstanceRequestBuilder is a builder class for
 * `google::spanner::admin::instance::v1::UpdateInstanceRequest`
 *
 * This is useful when calling
 * `google::cloud::spanner::InstanceAdminClient::UpdateInstance()` function.
 *
 * @par Example
 * @snippet samples.cc update-instance
 */
class UpdateInstanceRequestBuilder {
 public:
  /**
   * Constructs a `UpdateInstanceRequestBuilder`.
   */
  UpdateInstanceRequestBuilder() = default;

  // Copy and move.
  UpdateInstanceRequestBuilder(UpdateInstanceRequestBuilder const&) = default;
  UpdateInstanceRequestBuilder(UpdateInstanceRequestBuilder&&) = default;
  UpdateInstanceRequestBuilder& operator=(UpdateInstanceRequestBuilder const&) =
      default;
  UpdateInstanceRequestBuilder& operator=(UpdateInstanceRequestBuilder&&) =
      default;

  explicit UpdateInstanceRequestBuilder(std::string instance_name) {
    request_.mutable_instance()->set_name(std::move(instance_name));
  }
  explicit UpdateInstanceRequestBuilder(Instance const& in) {
    request_.mutable_instance()->set_name(in.FullName());
  }

  /**
   * Constructs `UpdateInstanceRequestBuilder` with
   * google::spanner::admin::instance::v1::Instance. It's particularly useful
   * if you want to add some labels to existing instances.
   */
  explicit UpdateInstanceRequestBuilder(
      google::spanner::admin::instance::v1::Instance in) {
    *request_.mutable_instance() = std::move(in);
  }

  UpdateInstanceRequestBuilder& SetName(std::string name) & {
    request_.mutable_instance()->set_name(std::move(name));
    return *this;
  }
  UpdateInstanceRequestBuilder&& SetName(std::string name) && {
    request_.mutable_instance()->set_name(std::move(name));
    return std::move(*this);
  }
  UpdateInstanceRequestBuilder& SetDisplayName(std::string display_name) & {
    SetDisplayNameImpl(std::move(display_name));
    return *this;
  }
  UpdateInstanceRequestBuilder&& SetDisplayName(std::string display_name) && {
    SetDisplayNameImpl(std::move(display_name));
    return std::move(*this);
  }
  UpdateInstanceRequestBuilder& SetNodeCount(int node_count) & {
    SetNodeCountImpl(node_count);
    return *this;
  }
  UpdateInstanceRequestBuilder&& SetNodeCount(int node_count) && {
    SetNodeCountImpl(node_count);
    return std::move(*this);
  }
  UpdateInstanceRequestBuilder& AddLabels(
      std::map<std::string, std::string> const& labels) & {
    AddLabelsImpl(labels);
    return *this;
  }
  UpdateInstanceRequestBuilder&& AddLabels(
      std::map<std::string, std::string> const& labels) && {
    AddLabelsImpl(labels);
    return std::move(*this);
  }
  UpdateInstanceRequestBuilder& SetLabels(
      std::map<std::string, std::string> const& labels) & {
    request_.mutable_instance()->clear_labels();
    AddLabelsImpl(labels);
    return *this;
  }
  UpdateInstanceRequestBuilder&& SetLabels(
      std::map<std::string, std::string> const& labels) && {
    request_.mutable_instance()->clear_labels();
    AddLabelsImpl(labels);
    return std::move(*this);
  }
  google::spanner::admin::instance::v1::UpdateInstanceRequest& Build() & {
    return request_;
  }
  google::spanner::admin::instance::v1::UpdateInstanceRequest&& Build() && {
    return std::move(request_);
  }

 private:
  google::spanner::admin::instance::v1::UpdateInstanceRequest request_;
  void SetDisplayNameImpl(std::string display_name) {
    if (!google::protobuf::util::FieldMaskUtil::IsPathInFieldMask(
            "display_name", request_.field_mask())) {
      request_.mutable_field_mask()->add_paths("display_name");
    }
    request_.mutable_instance()->set_display_name(std::move(display_name));
  }
  void SetNodeCountImpl(int node_count) {
    if (!google::protobuf::util::FieldMaskUtil::IsPathInFieldMask(
            "node_count", request_.field_mask())) {
      request_.mutable_field_mask()->add_paths("node_count");
    }
    request_.mutable_instance()->set_node_count(node_count);
  }
  void AddLabelsImpl(std::map<std::string, std::string> const& labels) {
    if (!google::protobuf::util::FieldMaskUtil::IsPathInFieldMask(
            "labels", request_.field_mask())) {
      request_.mutable_field_mask()->add_paths("labels");
    }
    for (auto const& pair : labels) {
      request_.mutable_instance()->mutable_labels()->insert(
          {pair.first, pair.second});
    }
  }
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_UPDATE_INSTANCE_REQUEST_BUILDER_H
