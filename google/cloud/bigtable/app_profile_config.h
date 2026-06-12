// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_APP_PROFILE_CONFIG_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_APP_PROFILE_CONFIG_H

#include "google/cloud/bigtable/version.h"
#include "google/bigtable/admin/v2/bigtable_instance_admin.pb.h"
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
/// Specify the initial configuration for an application profile.
class AppProfileConfig {
 public:
  explicit AppProfileConfig(
      google::bigtable::admin::v2::CreateAppProfileRequest proto)
      : proto_(std::move(proto)) {}

  /**
   * Create an AppProfile that uses multi-cluster routing.
   *
   * Read/write requests are routed to the nearest cluster in the instance,
   * and will fail over to the nearest cluster that is available in the event of
   * transient errors or delays. Clusters in a region are considered
   * equidistant. Choosing this option sacrifices read-your-writes consistency
   * to improve availability.
   *
   * @param profile_id The unique name of the AppProfile.
   * @param cluster_ids The set of clusters to route to. The order is
   *     ignored; clusters will be tried in order of distance. If left empty,
   *     all clusters are eligible.
   */
  static AppProfileConfig MultiClusterUseAny(
      std::string profile_id, std::vector<std::string> cluster_ids = {});

  /**
   * Create an AppProfile that uses single cluster routing.
   *
   * Unconditionally routes all read/write requests to a specific cluster. This
   * option preserves read-your-writes consistency but does not improve
   * availability.
   *
   * @param profile_id The unique name of the AppProfile.
   * @param cluster_id The cluster to which read/write requests are routed.
   * @param allow_transactional_writes Whether or not `CheckAndMutateRow` and
   *     `ReadModifyWriteRow` requests are allowed by this app profile. It is
   *     unsafe to send these requests to the same table/row/column in multiple
   *     clusters.
   */
  static AppProfileConfig SingleClusterRouting(
      std::string profile_id, std::string cluster_id,
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

  google::bigtable::admin::v2::CreateAppProfileRequest const& as_proto()
      const& {
    return proto_;
  }

  google::bigtable::admin::v2::CreateAppProfileRequest&& as_proto() && {
    return std::move(proto_);
  }

 private:
  AppProfileConfig() = default;

  google::bigtable::admin::v2::CreateAppProfileRequest proto_;
};

/// Build a proto to update an Application Profile configuration.
class AppProfileUpdateConfig {
 public:
  AppProfileUpdateConfig() = default;

  AppProfileUpdateConfig& set_ignore_warnings(bool value) {
    proto_.set_ignore_warnings(value);
    return *this;
  }
  AppProfileUpdateConfig& set_description(std::string description) {
    proto_.mutable_app_profile()->set_description(std::move(description));
    AddPathIfNotPresent("description");
    return *this;
  }
  AppProfileUpdateConfig& set_etag(std::string etag) {
    proto_.mutable_app_profile()->set_etag(std::move(etag));
    AddPathIfNotPresent("etag");
    return *this;
  }
  AppProfileUpdateConfig& set_multi_cluster_use_any(
      std::vector<std::string> cluster_ids = {}) {
    auto& mc_routing =
        *proto_.mutable_app_profile()->mutable_multi_cluster_routing_use_any();
    for (auto&& cluster_id : cluster_ids) {
      mc_routing.add_cluster_ids(std::move(cluster_id));
    }
    RemoveIfPresent("single_cluster_routing");
    AddPathIfNotPresent("multi_cluster_routing_use_any");
    return *this;
  }
  AppProfileUpdateConfig& set_single_cluster_routing(
      std::string const& cluster_id, bool allow_transactional_writes = false) {
    proto_.mutable_app_profile()
        ->mutable_single_cluster_routing()
        ->set_cluster_id(cluster_id);
    proto_.mutable_app_profile()
        ->mutable_single_cluster_routing()
        ->set_allow_transactional_writes(allow_transactional_writes);
    RemoveIfPresent("multi_cluster_routing_use_any");
    AddPathIfNotPresent("single_cluster_routing");
    return *this;
  }

  google::bigtable::admin::v2::UpdateAppProfileRequest const& as_proto()
      const& {
    return proto_;
  }

  google::bigtable::admin::v2::UpdateAppProfileRequest&& as_proto() && {
    return std::move(proto_);
  }

 private:
  void AddPathIfNotPresent(std::string field_name);
  void RemoveIfPresent(std::string const& field_name);

  google::bigtable::admin::v2::UpdateAppProfileRequest proto_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_APP_PROFILE_CONFIG_H
