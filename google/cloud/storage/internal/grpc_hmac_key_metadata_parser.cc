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

#include "google/cloud/storage/internal/grpc_hmac_key_metadata_parser.h"
#include "google/cloud/internal/time_utils.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

storage::HmacKeyMetadata FromProto(
    google::storage::v2::HmacKeyMetadata const& rhs) {
  storage::HmacKeyMetadata result;
  result.set_id(rhs.id());
  result.set_access_id(rhs.access_id());
  // The protos use `projects/{project}` format, but the field may be absent or
  // may have a project id (instead of number), so we need to do some parsing.
  // We are forgiving here. It is better to drop one field rather than dropping
  // the full message.
  absl::string_view project = rhs.project();
  absl::ConsumePrefix(&project, "projects/");
  result.set_project_id(std::string(project));
  result.set_service_account_email(rhs.service_account_email());
  result.set_state(rhs.state());
  result.set_time_created(
      google::cloud::internal::ToChronoTimePoint(rhs.create_time()));
  result.set_updated(
      google::cloud::internal::ToChronoTimePoint(rhs.update_time()));
  result.set_etag(rhs.etag());
  return result;
}

google::storage::v2::HmacKeyMetadata ToProto(
    storage::HmacKeyMetadata const& rhs) {
  google::storage::v2::HmacKeyMetadata result;
  result.set_id(rhs.id());
  result.set_access_id(rhs.access_id());
  result.set_project("projects/" + rhs.project_id());
  result.set_service_account_email(rhs.service_account_email());
  result.set_state(rhs.state());
  *result.mutable_create_time() =
      google::cloud::internal::ToProtoTimestamp(rhs.time_created());
  *result.mutable_update_time() =
      google::cloud::internal::ToProtoTimestamp(rhs.updated());
  result.set_etag(rhs.etag());
  return result;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
