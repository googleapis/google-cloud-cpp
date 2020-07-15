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

#include "google/cloud/pubsub/internal/emulator_overrides.h"
#include "google/cloud/internal/getenv.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

// Override connection endpoint and credentials with values appropriate for
// an emulated backend. This should be done after any user code that could
// also override the default values (i.e., immediately before establishing
// the connection).
pubsub::ConnectionOptions EmulatorOverrides(pubsub::ConnectionOptions options) {
  auto emulator_addr = google::cloud::internal::GetEnv("PUBSUB_EMULATOR_HOST");
  if (emulator_addr.has_value()) {
    options.set_endpoint(*emulator_addr)
        .set_credentials(grpc::InsecureChannelCredentials());
  }
  return options;
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
