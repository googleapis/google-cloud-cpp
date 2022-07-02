// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/internal/status_utils.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/internal/status_payload_keys.h"
#include "absl/strings/match.h"
#include <google/rpc/error_details.pb.h>
#include <google/rpc/status.pb.h>
#include <google/spanner/v1/spanner.pb.h>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

bool IsSessionNotFound(google::cloud::Status const& status) {
  if (status.code() != StatusCode::kNotFound) return false;

  // In the case of `kNotFound` errors, we can extract the resource
  // type from the `ResourceInfo` details in the original gRPC proto.
  google::rpc::Status proto;
  auto payload =
      internal::GetPayload(status, internal::kStatusPayloadGrpcProto);
  if (payload && proto.ParseFromString(*payload)) {
    google::rpc::ResourceInfo resource_info;
    for (google::protobuf::Any const& any : proto.details()) {
      if (any.UnpackTo(&resource_info)) {
        google::spanner::v1::Session session;
        auto session_url = "type.googleapis.com/" + session.GetTypeName();
        return resource_info.resource_type() == session_url;
      }
    }
  }

  // Without an attached `ResourceInfo` (which should never happen outside
  // of tests), we fallback to looking at the `Status` message.
  return absl::StrContains(status.message(), "Session not found");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
