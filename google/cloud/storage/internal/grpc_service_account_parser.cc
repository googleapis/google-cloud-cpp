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

#include "google/cloud/storage/internal/grpc_service_account_parser.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

google::storage::v2::GetServiceAccountRequest ToProto(
    storage::internal::GetProjectServiceAccountRequest const& request) {
  google::storage::v2::GetServiceAccountRequest proto;
  proto.set_project("projects/" + request.project_id());
  return proto;
}

storage::ServiceAccount FromProto(
    google::storage::v2::ServiceAccount const& meta) {
  storage::ServiceAccount result;
  result.set_email_address(meta.email_address());
  result.set_kind("storage#serviceAccount");
  return result;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
