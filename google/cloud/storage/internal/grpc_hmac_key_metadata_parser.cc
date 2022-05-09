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

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

HmacKeyMetadata GrpcHmacKeyMetadataParser::FromProto(
    google::storage::v2::HmacKeyMetadata const& rhs) {
  HmacKeyMetadata result;
  result.id_ = rhs.id();
  result.access_id_ = rhs.access_id();
  // The protos use `projects/{project}` format, but the field may be absent or
  // may have a project id (instead of number), we need to do some parsing. We
  // are forgiving here. It is better to drop one field rather than dropping
  // the full message.
  if (rhs.project().rfind("projects/", 0) == 0) {
    result.project_id_ = rhs.project().substr(std::strlen("projects/"));
  } else {
    result.project_id_ = rhs.project();
  }
  result.service_account_email_ = rhs.service_account_email();
  result.state_ = rhs.state();
  result.time_created_ =
      google::cloud::internal::ToChronoTimePoint(rhs.create_time());
  result.updated_ =
      google::cloud::internal::ToChronoTimePoint(rhs.update_time());

  return result;
}

google::storage::v2::HmacKeyMetadata GrpcHmacKeyMetadataParser::ToProto(
    HmacKeyMetadata const& rhs) {
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
  return result;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
