// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/async/handle_redirect_error.h"
#include "google/cloud/internal/status_payload_keys.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

void EnsureFirstMessageAppendObjectSpec(
    google::storage::v2::BidiWriteObjectRequest& request,
    google::rpc::Status const& rpc_status) {
  for (auto const& rpc_status_detail : rpc_status.details()) {
    google::storage::v2::BidiWriteObjectRedirectedError error =
        google::storage::v2::BidiWriteObjectRedirectedError{};
    if (!rpc_status_detail.UnpackTo(&error)) continue;
    if (!error.has_write_handle()) continue;
    if (request.has_write_object_spec()) {
      auto spec = request.write_object_spec();
      auto& append_object_spec = *request.mutable_append_object_spec();
      append_object_spec.set_bucket(spec.resource().bucket());
      append_object_spec.set_object(spec.resource().name());
      append_object_spec.set_if_metageneration_match(
          spec.if_metageneration_match());
      append_object_spec.set_if_metageneration_not_match(
          spec.if_metageneration_not_match());
    }
  }
}

google::rpc::Status ExtractGrpcStatus(Status const& status) {
  google::rpc::Status proto_status = google::rpc::Status{};
  auto payload = google::cloud::internal::GetPayload(
      status, google::cloud::internal::StatusPayloadGrpcProto());
  if (payload) proto_status.ParseFromString(*payload);
  return proto_status;
}

void ApplyRedirectErrors(google::storage::v2::BidiReadObjectSpec& spec,
                         google::rpc::Status const& rpc_status) {
  for (auto const& any : rpc_status.details()) {
    google::storage::v2::BidiReadObjectRedirectedError error =
        google::storage::v2::BidiReadObjectRedirectedError{};
    if (!any.UnpackTo(&error)) continue;
    *spec.mutable_read_handle() = std::move(*error.mutable_read_handle());
    *spec.mutable_routing_token() = std::move(*error.mutable_routing_token());
  }
}

void ApplyWriteRedirectErrors(google::storage::v2::AppendObjectSpec& spec,
                              google::rpc::Status const& rpc_status) {
  for (auto const& any : rpc_status.details()) {
    google::storage::v2::BidiWriteObjectRedirectedError error =
        google::storage::v2::BidiWriteObjectRedirectedError{};
    if (!any.UnpackTo(&error)) continue;
    *spec.mutable_write_handle() = std::move(*error.mutable_write_handle());
    *spec.mutable_routing_token() = std::move(*error.mutable_routing_token());
    if (error.has_generation()) spec.set_generation(error.generation());
  }
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
