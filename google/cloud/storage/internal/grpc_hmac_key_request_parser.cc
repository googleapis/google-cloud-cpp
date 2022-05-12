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

#include "google/cloud/storage/internal/grpc_hmac_key_request_parser.h"
#include "google/cloud/storage/internal/grpc_common_request_params.h"
#include "google/cloud/storage/internal/grpc_hmac_key_metadata_parser.h"
#include "google/cloud/storage/internal/openssl_util.h"

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

google::storage::v2::CreateHmacKeyRequest GrpcHmacKeyRequestParser::ToProto(
    CreateHmacKeyRequest const& request) {
  google::storage::v2::CreateHmacKeyRequest result;
  SetCommonParameters(result, request);
  result.set_project("projects/" + request.project_id());
  result.set_service_account_email(request.service_account());
  return result;
}

CreateHmacKeyResponse GrpcHmacKeyRequestParser::FromProto(
    google::storage::v2::CreateHmacKeyResponse const& response) {
  CreateHmacKeyResponse result;
  result.metadata = GrpcHmacKeyMetadataParser::FromProto(response.metadata());
  result.secret = internal::Base64Encode(response.secret_key_bytes());
  return result;
}

google::storage::v2::DeleteHmacKeyRequest GrpcHmacKeyRequestParser::ToProto(
    DeleteHmacKeyRequest const& request) {
  google::storage::v2::DeleteHmacKeyRequest result;
  SetCommonParameters(result, request);
  result.set_access_id(request.access_id());
  result.set_project("projects/" + request.project_id());
  return result;
}

google::storage::v2::GetHmacKeyRequest GrpcHmacKeyRequestParser::ToProto(
    GetHmacKeyRequest const& request) {
  google::storage::v2::GetHmacKeyRequest result;
  SetCommonParameters(result, request);
  result.set_access_id(request.access_id());
  result.set_project("projects/" + request.project_id());
  return result;
}

google::storage::v2::UpdateHmacKeyRequest GrpcHmacKeyRequestParser::ToProto(
    UpdateHmacKeyRequest const& request) {
  google::storage::v2::UpdateHmacKeyRequest result;
  SetCommonParameters(result, request);
  result.mutable_hmac_key()->set_access_id(request.access_id());
  result.mutable_hmac_key()->set_project("projects/" + request.project_id());
  result.mutable_hmac_key()->set_state(request.resource().state());
  result.mutable_update_mask()->add_paths("state");
  return result;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
