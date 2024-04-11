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
#include "google/cloud/internal/url_encode.h"
#include <regex>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

auto constexpr kIdempotencyTokenHeader = "x-goog-gcs-idempotency-token";

void AddIdempotencyToken(grpc::ClientContext& ctx,
                         rest_internal::RestContext const& context) {
  auto const& headers = context.headers();
  auto const l = headers.find(kIdempotencyTokenHeader);
  if (l != headers.end()) {
    for (auto const& v : l->second) {
      ctx.AddMetadata(kIdempotencyTokenHeader, v);
    }
  }
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
                         google::storage::v2::WriteObjectSpec const& spec) {
  context.AddMetadata(
      "x-goog-request-params",
      "bucket=" + google::cloud::internal::UrlEncode(spec.resource().bucket()));
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
