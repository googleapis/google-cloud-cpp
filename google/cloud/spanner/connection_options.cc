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
#include "google/cloud/spanner/internal/defaults.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/user_agent_prefix.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

std::string ConnectionOptionsTraits::default_endpoint() {
  return spanner_internal::DefaultOptions().get<EndpointOption>();
}

std::string ConnectionOptionsTraits::user_agent_prefix() {
  return internal::UserAgentPrefix();
}

int ConnectionOptionsTraits::default_num_channels() {
  static auto const kNumChannels =
      spanner_internal::DefaultOptions().get<GrpcNumChannelsOption>();
  return kNumChannels;
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
