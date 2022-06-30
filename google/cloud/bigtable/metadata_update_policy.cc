// Copyright 2018 Google Inc.
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

#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/internal/api_client_header.h"

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
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
    MetadataParamTypes const& metadata_param_type)
    : value_(metadata_param_type.type() + '=' + resource_name),
      api_client_header_(internal::ApiClientHeader()) {}

void MetadataUpdatePolicy::Setup(grpc::ClientContext& context) const {
  context.AddMetadata(std::string("x-goog-request-params"), value());
  context.AddMetadata(std::string("x-goog-api-client"), api_client_header());
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

bigtable::MetadataUpdatePolicy MakeMetadataUpdatePolicy(
    std::string const& table_name, std::string const& app_profile_id) {
  // The rule is the same for all RPCs in the Data API. We always include the
  // table name. We append an app profile id only if one was provided.
  return bigtable::MetadataUpdatePolicy(
      table_name +
          (app_profile_id.empty() ? "" : "&app_profile_id=" + app_profile_id),
      bigtable::MetadataParamTypes::TABLE_NAME);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
