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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INSTANCE_CONFIG_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INSTANCE_CONFIG_H

#include "google/cloud/bigtable/cluster_config.h"
#include "google/cloud/bigtable/version.h"
#include <google/bigtable/admin/v2/bigtable_instance_admin.pb.h>
#include <google/bigtable/admin/v2/common.pb.h>
#include <map>
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/// Specify the initial configuration for a new instance.
class InstanceConfig {
 public:
  InstanceConfig(std::string instance_id, std::string display_name,
                 std::map<std::string, ClusterConfig> clusters) {
    proto_.set_instance_id(std::move(instance_id));
    proto_.mutable_instance()->set_display_name(std::move(display_name));
    for (auto& kv : clusters) {
      (*proto_.mutable_clusters())[kv.first] = std::move(kv.second).as_proto();
    }
  }
  //@{
  /// @name Convenient shorthands for the instance types.
  using InstanceType = ::google::bigtable::admin::v2::Instance::Type;
  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr static InstanceType TYPE_UNSPECIFIED =
      google::bigtable::admin::v2::Instance::TYPE_UNSPECIFIED;
  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr static InstanceType PRODUCTION =
      google::bigtable::admin::v2::Instance::PRODUCTION;
  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr static InstanceType DEVELOPMENT =
      google::bigtable::admin::v2::Instance::DEVELOPMENT;
  //@}

  InstanceConfig& set_type(InstanceType type) {
    proto_.mutable_instance()->set_type(type);
    return *this;
  }

  InstanceConfig& insert_label(std::string const& key,
                               std::string const& value) {
    (*proto_.mutable_instance()->mutable_labels())[key] = value;
    return *this;
  }

  InstanceConfig& emplace_label(std::string const& key, std::string value) {
    (*proto_.mutable_instance()->mutable_labels())[key] = std::move(value);
    return *this;
  }

  // NOLINT: accessors can (and should) be snake_case.
  google::bigtable::admin::v2::CreateInstanceRequest const& as_proto() const& {
    return proto_;
  }

  // NOLINT: accessors can (and should) be snake_case.
  google::bigtable::admin::v2::CreateInstanceRequest&& as_proto() && {
    return std::move(proto_);
  }

 private:
  google::bigtable::admin::v2::CreateInstanceRequest proto_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INSTANCE_CONFIG_H
