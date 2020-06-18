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
#include "google/cloud/internal/compiler_info.h"
#include <sstream>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
// These we cannot change because they are part of the public API.
// NOLINTNEXTLINE(readability-identifier-naming)
MetadataParamTypes const MetadataParamTypes::PARENT("parent");
// NOLINTNEXTLINE(readability-identifier-naming)
MetadataParamTypes const MetadataParamTypes::NAME("name");
// NOLINTNEXTLINE(readability-identifier-naming)
MetadataParamTypes const MetadataParamTypes::RESOURCE("resource");
// NOLINTNEXTLINE(readability-identifier-naming)
MetadataParamTypes const MetadataParamTypes::TABLE_NAME("table_name");
// NOLINTNEXTLINE(readability-identifier-naming)
MetadataParamTypes const MetadataParamTypes::APP_PROFILE_NAME(
    "app_profile.name");
// NOLINTNEXTLINE(readability-identifier-naming)
MetadataParamTypes const MetadataParamTypes::INSTANCE_NAME("instance.name");
// NOLINTNEXTLINE(readability-identifier-naming)
MetadataParamTypes const MetadataParamTypes::BACKUP_NAME("backup.name");

MetadataUpdatePolicy::MetadataUpdatePolicy(
    std::string const& resource_name,
    MetadataParamTypes const& metadata_param_type) {
  std::string value = metadata_param_type.type();
  value += "=";
  value += resource_name;
  value_ = std::move(value);
  std::string api_client_header =
      "gl-cpp/" + google::cloud::internal::CompilerId() + "-" +
      google::cloud::internal::CompilerVersion() + "-" +
      google::cloud::internal::CompilerFeatures() + "-" +
      google::cloud::internal::LanguageVersion() + " gccl/" + version_string();
  api_client_header_ = std::move(api_client_header);
}

void MetadataUpdatePolicy::Setup(grpc::ClientContext& context) const {
  context.AddMetadata(std::string("x-goog-request-params"), value());
  context.AddMetadata(std::string("x-goog-api-client"), api_client_header());
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
