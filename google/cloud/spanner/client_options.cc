// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/spanner/client_options.h"
#include "google/cloud/internal/getenv.h"
#include <sstream>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
ConnectionOptions::ConnectionOptions(
    std::shared_ptr<grpc::ChannelCredentials> credentials)
    : credentials_(std::move(credentials)),
      endpoint_("spanner.googleapis.com") {
  auto tracing =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_ENABLE_TRACING");
  if (tracing.has_value()) {
    std::istringstream is{*tracing};
    std::string token;
    while (std::getline(is, token, ',')) {
      tracing_components_.insert(token);
    }
  }
  clog_enabled_ =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_ENABLE_CLOG")
          .has_value();
}

ConnectionOptions::ConnectionOptions()
    : ConnectionOptions(grpc::GoogleDefaultCredentials()) {}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
