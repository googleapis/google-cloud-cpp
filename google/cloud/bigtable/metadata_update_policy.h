// Copyright 2018 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_METADATA_UPDATE_POLICY_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_METADATA_UPDATE_POLICY_H_

#include "google/cloud/bigtable/version.h"
#include <grpcpp/grpcpp.h>
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Define the class for governing x-goog-request-params metadata value.
 *
 * The value of x-goog-request-params starts with one of the following suffix
 *    "parent=" : Operation in instance, e.g. TableAdmin::CreateTable.
 *    "table_name=" : table_id is known at the time of creation, e.g.
 *     Table::Apply.
 *    "name=" : this is used when table|_id is known only in the RPC call, e.g.
 *     TableAdmin::GetTable.
 *     "resource=" : this is used to set IAM policies for bigtable resource.
 *
 * The Setup function also adds x-goog-api-client header for analytics purpose.
 */
class MetadataParamTypes final {
 public:
  static MetadataParamTypes const PARENT;
  static MetadataParamTypes const NAME;
  static MetadataParamTypes const RESOURCE;
  static MetadataParamTypes const TABLE_NAME;

  std::string const& type() const { return type_; }

 private:
  std::string type_;
  MetadataParamTypes(std::string type) : type_(std::move(type)) {}
};

inline bool operator==(MetadataParamTypes const& lhs,
                       MetadataParamTypes const& rhs) {
  return lhs.type() == rhs.type();
}

inline bool operator!=(MetadataParamTypes const& lhs,
                       MetadataParamTypes const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

/// MetadataUpdatePolicy holds supported metadata and setup ClientContext
class MetadataUpdatePolicy {
 public:
  /**
   * Constructor with default metadata pair.
   *
   * @param resource_name hierarchical name of resource, including  project id,
   * instance id and/or table_id.
   * @param metadata_param_type type to decide prefix for the value of
   *     x-goog-request-params
   */
  MetadataUpdatePolicy(std::string const& resource_name,
                       MetadataParamTypes const& metadata_param_type);

  /**
   * Constructor with default metadata pair.
   *
   * @param resource_name hierarchical name of resource, including  project id,
   * instance id and/or table_id.
   * @param metadata_param_type type to decide prefix for the value of
   *     x-goog-request-params.
   * @param table_id table_id used in RPC call.
   */
  static MetadataUpdatePolicy FromTableId(
      std::string const& resource_name,
      MetadataParamTypes const& metadata_param_type,
      std::string const& table_id);

  // TODO(#2704) - this seems to be used only in tests, remove or use.
  /**
   * Constructor with default metadata pair.
   *
   * @param resource_name hierarchical name of resource, including  project id,
   * instance id and/or table_id.
   * @param metadata_param_type type to decide prefix for the value of
   *     x-goog-request-params.
   * @param cluster_id cluster_id of the cluster.
   */
  static MetadataUpdatePolicy FromClusterId(
      std::string const& resource_name,
      MetadataParamTypes const& metadata_param_type,
      std::string const& cluster_id);

  MetadataUpdatePolicy(MetadataUpdatePolicy&& rhs) noexcept = default;
  MetadataUpdatePolicy(MetadataUpdatePolicy const& rhs) = default;
  MetadataUpdatePolicy& operator=(MetadataUpdatePolicy const& rhs) = default;

  // Update the ClientContext for the next call.
  void Setup(grpc::ClientContext& context) const;

  std::string const& value() const { return value_; }
  std::string const& api_client_header() const { return api_client_header_; }

 private:
  std::string value_;
  std::string api_client_header_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_METADATA_UPDATE_POLICY_H_
