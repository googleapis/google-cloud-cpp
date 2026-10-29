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

#include "google/cloud/storage/internal/grpc/configure_client_context.h"
#include "google/cloud/storage/internal/tracked_features.h"
#include "google/cloud/internal/url_encode.h"
#include <array>
#include <cstdint>
#include <regex>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

auto constexpr kIdempotencyTokenHeader = "x-goog-gcs-idempotency-token";

// The `RestContext` headers forwarded to gRPC as metadata. Anything else on
// the `RestContext` is specific to the REST transport.
constexpr std::array<char const*, 2> kForwardedHeaders{kIdempotencyTokenHeader,
                                                       kFeaturesHeader};

void ApplyRoutingHeadersImpl(grpc::ClientContext& context,
                             std::string const& bucket_name,
                             RoutingHeaderOptions const& options) {
  std::string params =
      "bucket=" + google::cloud::internal::UrlEncode(bucket_name);
  if (!options.routing_token.empty()) {
    params += "&routing_token=" +
              google::cloud::internal::UrlEncode(options.routing_token);
  }
  context.AddMetadata("x-goog-request-params", std::move(params));
}

// Copies every value of the @p name header from @p context into @p ctx.
void CopyHeader(grpc::ClientContext& ctx,
                rest_internal::RestContext const& context,
                std::string const& name) {
  auto const& headers = context.headers();
  auto const l = headers.find(name);
  if (l == headers.end()) return;
  for (std::string const& v : l->second.values()) ctx.AddMetadata(name, v);
}

}  // namespace

void ApplyRestContextHeaders(grpc::ClientContext& ctx,
                             rest_internal::RestContext const& context) {
  for (char const* name : kForwardedHeaders) CopyHeader(ctx, context, name);
}

void AddIdempotencyToken(grpc::ClientContext& ctx, std::string const& token) {
  ctx.AddMetadata(kIdempotencyTokenHeader, token);
}

void AddFeaturesHeader(grpc::ClientContext& ctx, std::uint32_t mask) {
  if (mask == 0) return;
  ctx.AddMetadata(kFeaturesHeader, EncodeFeatureMask(mask));
}

void ApplyRoutingHeaders(
    grpc::ClientContext& context,
    storage::internal::InsertObjectMediaRequest const& request) {
  context.AddMetadata(
      "x-goog-request-params",
      "bucket=" + google::cloud::internal::UrlEncode("projects/_/buckets/" +
                                                     request.bucket_name()));
}

void ApplyRoutingHeaders(grpc::ClientContext& context,
                         google::storage::v2::WriteObjectSpec const& spec,
                         RoutingHeaderOptions const& options) {
  ApplyRoutingHeadersImpl(context, spec.resource().bucket(), options);
}

void ApplyRoutingHeaders(grpc::ClientContext& context,
                         google::storage::v2::AppendObjectSpec const& spec,
                         RoutingHeaderOptions const& options) {
  ApplyRoutingHeadersImpl(context, spec.bucket(), options);
}

void ApplyRoutingHeaders(grpc::ClientContext& context,
                         storage::internal::UploadChunkRequest const& request) {
  ApplyResumableUploadRoutingHeader(context, request.upload_session_url());
}

void ApplyResumableUploadRoutingHeader(grpc::ClientContext& context,
                                       std::string const& upload_id) {
  static auto* re =
      new std::regex{"(projects/[^/]+/buckets/[^/]+)/.*", std::regex::optimize};
  std::smatch match;
  if (!std::regex_match(upload_id, match, *re)) return;
  context.AddMetadata(
      "x-goog-request-params",
      "bucket=" + google::cloud::internal::UrlEncode(match[1].str()));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
