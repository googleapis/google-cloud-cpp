// Copyright 2024 Google LLC
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

#include "google/cloud/internal/rest_set_metadata.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/rest_options.h"
#include "google/cloud/rest_options.h"

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

void SetMetadata(RestContext& context, Options const& options,
                 std::vector<std::string> const& request_params,
                 std::string const& api_client_header) {
  context.AddHeader("x-goog-api-client", api_client_header);
  if (!request_params.empty()) {
    context.AddHeader("x-goog-request-params",
                      absl::StrJoin(request_params, "&"));
  }
  if (options.has<UserProjectOption>()) {
    context.AddHeader("x-goog-user-project", options.get<UserProjectOption>());
  }
  if (options.has<QuotaUserOption>()) {
    context.AddHeader("x-goog-quota-user", options.get<QuotaUserOption>());
  }
  if (options.has<FieldMaskOption>()) {
    context.AddHeader("x-goog-fieldmask", options.get<FieldMaskOption>());
  }
  if (options.has<ServerTimeoutOption>()) {
    auto ms_rep = absl::StrCat(
        absl::Dec(options.get<ServerTimeoutOption>().count(), absl::kZeroPad4));
    context.AddHeader("x-server-timeout",
                      ms_rep.insert(ms_rep.size() - 3, "."));
  }
  for (auto const& h : options.get<CustomHeadersOption>()) {
    context.AddHeader(h.first, h.second);
  }
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
