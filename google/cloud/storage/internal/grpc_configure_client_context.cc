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

#include "google/cloud/storage/internal/grpc_configure_client_context.h"
#include <regex>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

void ApplyRoutingHeaders(grpc::ClientContext& context,
                         InsertObjectMediaRequest const& request) {
  context.AddMetadata("x-goog-request-params",
                      "bucket=projects/_/buckets/" + request.bucket_name());
}

void ApplyRoutingHeaders(grpc::ClientContext& context,
                         UploadChunkRequest const& request) {
  static auto* bucket_regex =
      new std::regex{"(projects/[^/]+/buckets/[^/]+)/.*", std::regex::optimize};
  std::smatch match;
  if (std::regex_match(request.upload_session_url(), match, *bucket_regex)) {
    context.AddMetadata("x-goog-request-params", "bucket=" + match[1].str());
  }
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
