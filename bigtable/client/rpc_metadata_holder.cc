// Copyright 2017 Google Inc.
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

#include "bigtable/client/rpc_metadata_holder.h"
#include "bigtable/client/internal/make_unique.h"

#include <sstream>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
std::unique_ptr<RPCMetadataHolder> DefaultRPCMetadataHolder(
    std::string reource_name, RPCRequestParamType request_param_type) {
  RPCMetadataHolder* rpc_metadata_holder =
      new RPCMetadataHolder(reource_name, request_param_type);
  return std::unique_ptr<RPCMetadataHolder>(rpc_metadata_holder);
}

std::unique_ptr<RPCMetadataHolder> RPCMetadataHolder::clone() const {
  return bigtable::internal::make_unique<RPCMetadataHolder>(
      new RPCMetadataHolder(this->resource_name_, this->request_param_type_));
}

std::unique_ptr<RPCMetadataHolder> RPCMetadataHolder::cloneWithModifications(
    RPCRequestParamType request_param_type, std::string table_id) const {
  return bigtable::internal::make_unique<RPCMetadataHolder>(
      new RPCMetadataHolder(this->resource_name_, request_param_type,
                            table_id));
}

void RPCMetadataHolder::setup(grpc::ClientContext& context) const {
  context.AddMetadata(google_cloud_resource_prefix_.get_key(),
                      google_cloud_resource_prefix_.get_value());
  context.AddMetadata(x_google_request_params_.get_key(),
                      x_google_request_params_.get_value());
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
