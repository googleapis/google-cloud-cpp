// Copyright 2020 Google LLC
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

#include "google/cloud/pubsub/connection_options.h"
#include "google/cloud/pubsub/internal/user_agent_prefix.h"
#include <thread>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

std::string ConnectionOptionsTraits::default_endpoint() {
  return "pubsub.googleapis.com";
}

std::string ConnectionOptionsTraits::user_agent_prefix() {
  return pubsub_internal::UserAgentPrefix();
}

int ConnectionOptionsTraits::default_num_channels() {
  auto const ncores = std::thread::hardware_concurrency();
  return ncores == 0 ? 4 : static_cast<int>(ncores * 4);
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
