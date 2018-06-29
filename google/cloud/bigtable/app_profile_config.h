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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_APP_PROFILE_CONFIG_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_APP_PROFILE_CONFIG_H_

#include "google/cloud/bigtable/bigtable_strong_types.h"
#include "google/cloud/bigtable/table_strong_types.h"
#include <google/bigtable/admin/v2/bigtable_instance_admin.pb.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/// Specify the initial configuration for an application profile.
class AppProfileConfig {
 public:
  explicit AppProfileConfig(
      google::bigtable::admin::v2::CreateAppProfileRequest proto)
      : proto_(std::move(proto)) {}

  static AppProfileConfig MultiClusterUseAny(AppProfileId profile_id);
  static AppProfileConfig SingleClusterRouting(
      AppProfileId profile_id, ClusterId cluster_id,
      bool allow_transactional_writes = false);

  AppProfileConfig& set_ignore_warnings(bool value) {
    proto_.set_ignore_warnings(value);
    return *this;
  }

  AppProfileConfig& set_description(std::string description) {
    proto_.mutable_app_profile()->set_description(std::move(description));
    return *this;
  }

  AppProfileConfig& set_etag(std::string etag) {
    proto_.mutable_app_profile()->set_etag(std::move(etag));
    return *this;
  }

  // NOLINT: accessors can (and should) be snake_case.
  google::bigtable::admin::v2::CreateAppProfileRequest const& as_proto() const {
    return proto_;
  }

  // NOLINT: accessors can (and should) be snake_case.
  google::bigtable::admin::v2::CreateAppProfileRequest as_proto_move() {
    return std::move(proto_);
  }

 private:
  AppProfileConfig() : proto_() {}

 private:
  google::bigtable::admin::v2::CreateAppProfileRequest proto_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_APP_PROFILE_CONFIG_H_
