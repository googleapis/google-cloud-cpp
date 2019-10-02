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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_UPDATE_INSTANCE_REQUEST_BUILDER_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_UPDATE_INSTANCE_REQUEST_BUILDER_H_

#include "google/cloud/spanner/instance.h"
#include "google/cloud/spanner/version.h"
#include <google/spanner/admin/instance/v1/spanner_instance_admin.pb.h>
#include <map>

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
  explicit UpdateInstanceRequestBuilder(Instance in) {
    request_.mutable_instance()->set_name(in.FullName());
  }

  /**
   * Constructs `UpdateInstanceRequestBuilder` with
   * google::spanner::admin::instance::v1::Instance. It's particulaly useful if
   * you want to add some labels to existing instances.
   */
  explicit UpdateInstanceRequestBuilder(
      google::spanner::admin::instance::v1::Instance in) {
    *request_.mutable_instance() = std::move(in);
  }

  UpdateInstanceRequestBuilder& SetName(std::string name) {
    request_.mutable_instance()->set_name(std::move(name));
    return *this;
  }
  UpdateInstanceRequestBuilder& SetDisplayName(std::string);
  UpdateInstanceRequestBuilder& SetNodeCount(int);
  UpdateInstanceRequestBuilder& AddLabels(
      std::map<std::string, std::string> const&);
  UpdateInstanceRequestBuilder& SetLabels(
      std::map<std::string, std::string> const&);
  google::spanner::admin::instance::v1::UpdateInstanceRequest& Build() & {
    return request_;
  }
  google::spanner::admin::instance::v1::UpdateInstanceRequest&& Build() && {
    return std::move(request_);
  }

 private:
  google::spanner::admin::instance::v1::UpdateInstanceRequest request_;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_UPDATE_INSTANCE_REQUEST_BUILDER_H_
