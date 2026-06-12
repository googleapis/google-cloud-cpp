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

BidiWriteRedirectInfo HandleBidiWriteRedirect(
    google::storage::v2::BidiWriteObjectRequest& request,
    google::rpc::Status const& rpc_status) {
  BidiWriteRedirectInfo info;
  std::optional<google::storage::v2::BidiWriteObjectRedirectedError>
      redirect_error;
  for (auto const& rpc_status_detail : rpc_status.details()) {
    google::storage::v2::BidiWriteObjectRedirectedError error_proto;
    if (rpc_status_detail.UnpackTo(&error_proto)) {
      redirect_error = std::move(error_proto);
      break;  // Found the redirect error, no need to look further.
    }
  }
  if (!redirect_error) {
    return info;
  }

  // We always extract the routing token if it's provided, as it's needed for
  // the x-goog-request-params header in the next retry attempt.
  if (!redirect_error->routing_token().empty()) {
    info.routing_token = redirect_error->routing_token();
  }
  if (!redirect_error->has_write_handle()) {
    return info;
  }

  // If we get back a write handle, we should use it. We can only use it
  // on an append object spec. If we have a write object spec, we copy the
  // relevant fields from write object spec to append object spec.
  // If we have an append object spec, we copy the relevant fields from the
  // error to the spec.
  if (request.has_write_object_spec()) {
    auto write_object_spec = request.write_object_spec();
    auto& append_object_spec = *request.mutable_append_object_spec();
    append_object_spec.set_bucket(write_object_spec.resource().bucket());
    append_object_spec.set_object(write_object_spec.resource().name());
    append_object_spec.set_if_metageneration_match(
        write_object_spec.if_metageneration_match());
    append_object_spec.set_if_metageneration_not_match(
        write_object_spec.if_metageneration_not_match());
  }
  if (request.has_append_object_spec()) {
    auto& append_object_spec = *request.mutable_append_object_spec();
    *append_object_spec.mutable_write_handle() =
        std::move(*redirect_error->mutable_write_handle());
    *append_object_spec.mutable_routing_token() =
        std::move(*redirect_error->mutable_routing_token());
    if (redirect_error->has_generation())
      append_object_spec.set_generation(redirect_error->generation());
  }
  return info;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
