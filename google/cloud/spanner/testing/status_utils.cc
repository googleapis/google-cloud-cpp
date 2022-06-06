// Copyright 2022 Google LLC
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

#include "google/cloud/spanner/testing/status_utils.h"
#include "google/cloud/grpc_error_delegate.h"
#include <google/rpc/error_details.pb.h>
#include <google/rpc/status.pb.h>
#include <google/spanner/v1/spanner.pb.h>

namespace google {
namespace cloud {
namespace spanner_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

grpc::Status SessionNotFoundRpcError(std::string name) {
  google::spanner::v1::Session session;
  auto session_url = "type.googleapis.com/" + session.GetTypeName();

  google::rpc::ResourceInfo resource_info;
  resource_info.set_resource_type(session_url);
  *resource_info.mutable_resource_name() = std::move(name);
  resource_info.set_description("Session does not exist.");

  google::rpc::Status proto;
  proto.set_code(grpc::StatusCode::NOT_FOUND);
  proto.set_message("Session not found");
  proto.add_details()->PackFrom(resource_info);

  return grpc::Status(grpc::StatusCode::NOT_FOUND, proto.message(),
                      proto.SerializeAsString());
}

Status SessionNotFoundError(std::string name) {
  return MakeStatusFromRpcError(SessionNotFoundRpcError(std::move(name)));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google
