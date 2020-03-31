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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_CREATE_INSTANCE_REQUEST_BUILDER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_CREATE_INSTANCE_REQUEST_BUILDER_H

#include "google/cloud/spanner/instance.h"
#include "google/cloud/spanner/version.h"
#include <google/spanner/admin/instance/v1/spanner_instance_admin.pb.h>
#include <map>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * CreateInstanceRequestBuilder is a builder class for
 * `google::spanner::admin::instance::v1::CreateInstanceRequest`
 *
 * This is useful when calling the `InstanceAdminClient::CreateInstance()`
 * function.
 *
 * @par Example
 * @snippet samples.cc create-instance
 */
class CreateInstanceRequestBuilder {
 public:
  // Copy and move.
  CreateInstanceRequestBuilder(CreateInstanceRequestBuilder const&) = default;
  CreateInstanceRequestBuilder(CreateInstanceRequestBuilder&&) = default;
  CreateInstanceRequestBuilder& operator=(CreateInstanceRequestBuilder const&) =
      default;
  CreateInstanceRequestBuilder& operator=(CreateInstanceRequestBuilder&&) =
      default;

  /**
   * Constructor requires Instance and Cloud Spanner instance config name. It
   * sets node_count = 1, and display_name = instance_id as the default values.
   */
  CreateInstanceRequestBuilder(Instance const& in, std::string config) {
    request_.set_parent("projects/" + in.project_id());
    request_.set_instance_id(in.instance_id());
    request_.mutable_instance()->set_name(in.FullName());
    request_.mutable_instance()->set_display_name(in.instance_id());
    request_.mutable_instance()->set_node_count(1);
    request_.mutable_instance()->set_config(std::move(config));
  }

  CreateInstanceRequestBuilder& SetDisplayName(std::string display_name) & {
    request_.mutable_instance()->set_display_name(std::move(display_name));
    return *this;
  }

  CreateInstanceRequestBuilder&& SetDisplayName(std::string display_name) && {
    request_.mutable_instance()->set_display_name(std::move(display_name));
    return std::move(*this);
  }

  CreateInstanceRequestBuilder& SetNodeCount(int node_count) & {
    request_.mutable_instance()->set_node_count(node_count);
    return *this;
  }

  CreateInstanceRequestBuilder&& SetNodeCount(int node_count) && {
    request_.mutable_instance()->set_node_count(node_count);
    return std::move(*this);
  }

  CreateInstanceRequestBuilder& SetLabels(
      std::map<std::string, std::string> const& labels) & {
    for (auto const& pair : labels) {
      request_.mutable_instance()->mutable_labels()->insert(
          {pair.first, pair.second});
    }
    return *this;
  }

  CreateInstanceRequestBuilder&& SetLabels(
      std::map<std::string, std::string> const& labels) && {
    for (auto const& pair : labels) {
      request_.mutable_instance()->mutable_labels()->insert(
          {pair.first, pair.second});
    }
    return std::move(*this);
  }

  google::spanner::admin::instance::v1::CreateInstanceRequest& Build() & {
    return request_;
  }
  google::spanner::admin::instance::v1::CreateInstanceRequest&& Build() && {
    return std::move(request_);
  }

 private:
  google::spanner::admin::instance::v1::CreateInstanceRequest request_;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_CREATE_INSTANCE_REQUEST_BUILDER_H
