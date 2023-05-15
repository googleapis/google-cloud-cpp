// Copyright 2023 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_PROJECT_REQUEST_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_PROJECT_REQUEST_H

#include "google/cloud/internal/rest_request.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/tracing_options.h"
#include "google/cloud/version.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include <string>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class ListProjectsRequest {
 public:
  ListProjectsRequest() = default;

  std::int32_t const& max_results() const { return max_results_; }
  std::string const& page_token() const { return page_token_; }

  ListProjectsRequest& set_max_results(std::int32_t max_results) & {
    max_results_ = max_results;
    return *this;
  }
  ListProjectsRequest&& set_max_results(std::int32_t max_results) && {
    return std::move(set_max_results(max_results));
  }

  ListProjectsRequest& set_page_token(std::string page_token) & {
    page_token_ = std::move(page_token);
    return *this;
  }
  ListProjectsRequest&& set_page_token(std::string page_token) && {
    return std::move(set_page_token(std::move(page_token)));
  }

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;

 private:
  std::int32_t max_results_{0};
  std::string page_token_;
};

// Builds RestRequest from ListProjectsRequest.
StatusOr<rest_internal::RestRequest> BuildRestRequest(
    ListProjectsRequest const& r);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_PROJECT_REQUEST_H
