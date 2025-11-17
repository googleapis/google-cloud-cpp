// Copyright 2025 Google LLC
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

#include "google/cloud/bigtable/internal/retry_traits.h"
#include "google/cloud/internal/status_payload_keys.h"
#include "google/cloud/status.h"
#include "absl/strings/match.h"
#include <google/rpc/error_details.pb.h>
#include <google/rpc/status.pb.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

bool QueryPlanRefreshRetry::IsQueryPlanExpired(Status const& s) {
  if (s.code() == StatusCode::kFailedPrecondition) {
    if (absl::StrContains(s.message(), "PREPARED_QUERY_EXPIRED")) {
      return true;
    }
    auto payload = google::cloud::internal::GetPayload(
        s, google::cloud::internal::StatusPayloadGrpcProto());
    google::rpc::Status proto;
    if (payload && proto.ParseFromString(*payload)) {
      google::rpc::PreconditionFailure failure;
      for (google::protobuf::Any const& any : proto.details()) {
        if (any.UnpackTo(&failure)) {
          for (auto const& v : failure.violations()) {
            if (absl::StrContains(v.type(), "PREPARED_QUERY_EXPIRED")) {
              return true;
            }
          }
        }
      }
    }
  }
  return false;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
