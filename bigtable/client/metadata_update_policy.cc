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

#include "bigtable/client/metadata_update_policy.h"
#include "bigtable/client/internal/make_unique.h"

#include <sstream>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
MetadataParamTypes const MetadataParamTypes::PARENT("parent");
MetadataParamTypes const MetadataParamTypes::NAME("name");
MetadataParamTypes const MetadataParamTypes::TABLE_NAME("table_name");

MetadataUpdatePolicy::MetadataUpdatePolicy(
    std::string resource_name, MetadataParamTypes metadata_param_type) {
  std::string value = metadata_param_type.type();
  value += "=";
  value += resource_name;
  x_google_request_params_ =
      std::make_pair("x-goog-request-params", std::move(value));
}

MetadataUpdatePolicy::MetadataUpdatePolicy(
    std::string resource_name, MetadataParamTypes metadata_param_type,
    std::string table_id) {
  std::string value = metadata_param_type.type();
  value += "=";
  value += resource_name;
  value += "/tables/" + table_id;
  x_google_request_params_ =
      std::make_pair("x-goog-request-params", std::move(value));
}

MetadataUpdatePolicy::MetadataUpdatePolicy(MetadataUpdatePolicy const& rhs) {
  x_google_request_params_ = rhs.x_google_request_params_;
}

void MetadataUpdatePolicy::setup(grpc::ClientContext& context) const {
  context.AddMetadata(x_google_request_params_.first,
                      x_google_request_params_.second);
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
