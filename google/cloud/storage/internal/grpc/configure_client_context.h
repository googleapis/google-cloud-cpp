// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_CONFIGURE_CLIENT_CONTEXT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_CONFIGURE_CLIENT_CONTEXT_H

#include "google/cloud/storage/internal/generic_request.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/rest_context.h"
#include <google/storage/v2/storage.pb.h>
#include <grpcpp/client_context.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// Configures @p ctx using @p context.
void AddIdempotencyToken(grpc::ClientContext& ctx,
                         rest_internal::RestContext const& context);

/**
 * Inject request query parameters into grpc::ClientContext.
 *
 * The REST API has a number of "standard" query parameters that are not part of
 * the gRPC request body, instead they are sent via metadata headers in the gRPC
 * request.
 *
 * @see https://cloud.google.com/apis/docs/system-parameters
 */
template <typename Request>
void ApplyQueryParameters(grpc::ClientContext& ctx, Options const& options,
                          Request const& request) {
  // The gRPC API has a single field for the `QuotaUser` parameter, while the
  // JSON API has two:
  //    https://cloud.google.com/storage/docs/json_api/v1/parameters#quotaUser
  // Fortunately the semantics are to use `quotaUser` if set, so we can set
  // the `UserIp` value into the `quota_user` field, and overwrite it if
  // `QuotaUser` is also set. A bit bizarre, but at least it is backwards
  // compatible.
  if (request.template HasOption<storage::QuotaUser>()) {
    ctx.AddMetadata("x-goog-quota-user",
                    request.template GetOption<storage::QuotaUser>().value());
  } else if (request.template HasOption<storage::UserIp>()) {
    ctx.AddMetadata("x-goog-quota-user",
                    request.template GetOption<storage::UserIp>().value());
  }

  if (request.template HasOption<storage::Fields>()) {
    ctx.AddMetadata("x-goog-fieldmask",
                    request.template GetOption<storage::Fields>().value());
  }
  internal::ConfigureContext(ctx, options);
}

/**
 * The generated `StorageMetadata` stub can not handle dynamic routing headers
 * for client side streaming. So we manually match and extract the headers in
 * this function.
 */
void ApplyRoutingHeaders(
    grpc::ClientContext& context,
    storage::internal::InsertObjectMediaRequest const& request);

/// @copydoc ApplyRoutingHeaders(grpc::ClientContext&,)
void ApplyRoutingHeaders(grpc::ClientContext& context,
                         google::storage::v2::WriteObjectSpec const& spec);

/**
 * The generated `StorageMetadata` stub can not handle dynamic routing headers
 * for client side streaming. So we manually match and extract the headers in
 * this function.
 */
void ApplyRoutingHeaders(grpc::ClientContext& context,
                         storage::internal::UploadChunkRequest const& request);

/**
 * The generated `StorageMetadata` stub can not handle dynamic routing headers
 * for bi-directional streaming. So we manually match and extract the headers in
 * this function.
 */
void ApplyResumableUploadRoutingHeader(grpc::ClientContext& context,
                                       std::string const& upload_id);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_CONFIGURE_CLIENT_CONTEXT_H
