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
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

google::storage::v2::GetServiceAccountRequest GrpcServiceAccountParser::ToProto(
    GetProjectServiceAccountRequest const& request) {
  google::storage::v2::GetServiceAccountRequest proto;
  proto.set_project("projects/" + request.project_id());
  return proto;
}

ServiceAccount GrpcServiceAccountParser::FromProto(
    google::storage::v2::ServiceAccount const& meta) {
  ServiceAccount result;
  result.email_address_ = meta.email_address();
  result.kind_ = "storage#serviceAccount";
  return result;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
