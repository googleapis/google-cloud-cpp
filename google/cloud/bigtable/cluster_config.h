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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_CLUSTER_CONFIG_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_CLUSTER_CONFIG_H

#include "google/cloud/bigtable/version.h"
#include <google/bigtable/admin/v2/bigtable_instance_admin.pb.h>
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {

/// Specify the initial configuration for a new cluster.
class ClusterConfig {
 public:
  using StorageType = google::bigtable::admin::v2::StorageType;
  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr static StorageType STORAGE_TYPE_UNSPECIFIED =
      google::bigtable::admin::v2::STORAGE_TYPE_UNSPECIFIED;
  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr static StorageType SSD = google::bigtable::admin::v2::SSD;
  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr static StorageType HDD = google::bigtable::admin::v2::HDD;

  // NOLINTNEXTLINE(google-explicit-constructor)
  ClusterConfig(google::bigtable::admin::v2::Cluster cluster)
      : proto_(std::move(cluster)) {}

  ClusterConfig(std::string location, std::int32_t serve_nodes,
                StorageType storage) {
    proto_.set_location(std::move(location));
    proto_.set_serve_nodes(serve_nodes);
    proto_.set_default_storage_type(storage);
  }

  ClusterConfig& SetEncryptionConfig(
      google::bigtable::admin::v2::Cluster::EncryptionConfig encryption) & {
    *proto_.mutable_encryption_config() = std::move(encryption);
    return *this;
  }
  ClusterConfig&& SetEncryptionConfig(
      google::bigtable::admin::v2::Cluster::EncryptionConfig encryption) && {
    return std::move(SetEncryptionConfig(std::move(encryption)));
  }

  std::string const& GetName() { return proto_.name(); }

  google::bigtable::admin::v2::Cluster const& as_proto() const& {
    return proto_;
  }

  google::bigtable::admin::v2::Cluster&& as_proto() && {
    return std::move(proto_);
  }

 private:
  google::bigtable::admin::v2::Cluster proto_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_CLUSTER_CONFIG_H
