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

#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/internal/build_info.h"
#include <sstream>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
MetadataParamTypes const MetadataParamTypes::PARENT("parent");
MetadataParamTypes const MetadataParamTypes::NAME("name");
MetadataParamTypes const MetadataParamTypes::RESOURCE("resource");
MetadataParamTypes const MetadataParamTypes::TABLE_NAME("table_name");

MetadataUpdatePolicy::MetadataUpdatePolicy(
    std::string const& resource_name,
    MetadataParamTypes const& metadata_param_type) {
  std::string value = metadata_param_type.type();
  value += "=";
  value += resource_name;
  value_ = std::move(value);
  std::string api_client_header = "gl-cpp/" +
                                  google::cloud::internal::language_version() +
                                  " gccl/" + version_string();
  api_client_header_ = std::move(api_client_header);
}

MetadataUpdatePolicy MetadataUpdatePolicy::FromTableId(
    std::string const& resource_name,
    MetadataParamTypes const& metadata_param_type,
    std::string const& table_id) {
  MetadataUpdatePolicy policy(resource_name, metadata_param_type);
  policy.value_ += "/tables/" + table_id;
  return policy;
}

MetadataUpdatePolicy MetadataUpdatePolicy::FromClusterId(
    std::string const& resource_name,
    MetadataParamTypes const& metadata_param_type,
    std::string const& cluster_id) {
  MetadataUpdatePolicy policy(resource_name, metadata_param_type);
  policy.value_ += "/clusters/" + cluster_id;
  return policy;
}

void MetadataUpdatePolicy::Setup(grpc::ClientContext& context) const {
  context.AddMetadata(std::string("x-goog-request-params"), value());
  context.AddMetadata(std::string("x-goog-api-client"), api_client_header());
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
