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

#include "google/cloud/spanner/connection_options.h"
#include "google/cloud/spanner/internal/compiler_info.h"
#include "google/cloud/internal/getenv.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

std::string ConnectionOptionsTraits::default_endpoint() {
  auto default_endpoint = google::cloud::internal::GetEnv(
      "GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_ENDPOINT");
  return default_endpoint.has_value() ? *default_endpoint
                                      : "spanner.googleapis.com";
}

std::string ConnectionOptionsTraits::user_agent_prefix() {
  return "gcloud-cpp/" + google::cloud::spanner::VersionString() + " (" +
         internal::CompilerId() + "-" + internal::CompilerVersion() + "; " +
         internal::CompilerFeatures() + ")";
}

int ConnectionOptionsTraits::default_num_channels() { return 4; }

namespace internal {

// Override connection endpoint and credentials with values appropriate for
// an emulated backend. This should be done after any user code that could
// also override the default values (i.e., immediately before establishing
// the connection).
ConnectionOptions EmulatorOverrides(ConnectionOptions options) {
  auto emulator_addr = google::cloud::internal::GetEnv("SPANNER_EMULATOR_HOST");
  if (emulator_addr.has_value()) {
    options.set_endpoint(*emulator_addr)
        .set_credentials(grpc::InsecureChannelCredentials());
  }
  return options;
}

}  // namespace internal

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
