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

#include "google/cloud/internal/populate_grpc_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/getenv.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

Options PopulateGrpcOptions(Options opts, std::string const& emulator_env_var) {
  if (!emulator_env_var.empty()) {
    auto e = GetEnv(emulator_env_var.c_str());
    if (e && !e->empty()) {
      opts.set<GrpcCredentialOption>(grpc::InsecureChannelCredentials());
    }
  }
  if (!opts.has<GrpcCredentialOption>() &&
      !opts.has<UnifiedCredentialsOption>()) {
    opts.set<GrpcCredentialOption>(grpc::GoogleDefaultCredentials());
  }
  if (!opts.has<GrpcTracingOptionsOption>()) {
    opts.set<GrpcTracingOptionsOption>(DefaultTracingOptions());
  }
  return opts;
}

TracingOptions DefaultTracingOptions() {
  auto tracing_options = GetEnv("GOOGLE_CLOUD_CPP_TRACING_OPTIONS");
  if (!tracing_options) return {};
  return TracingOptions{}.SetOptions(*tracing_options);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
