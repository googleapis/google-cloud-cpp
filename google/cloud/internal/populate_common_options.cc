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

#include "google/cloud/internal/populate_common_options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/user_agent_prefix.h"
#include "google/cloud/opentelemetry_options.h"
#include "absl/strings/str_split.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

Options PopulateCommonOptions(Options opts, std::string const& endpoint_env_var,
                              std::string const& emulator_env_var,
                              std::string const& authority_env_var,
                              std::string default_endpoint) {
  // This check relies on Options::get<std::string> returning an empty string as
  // a default value.
  if (opts.get<EndpointOption>().empty()) {
    opts.set<EndpointOption>(default_endpoint);
  }
  if (!endpoint_env_var.empty()) {
    auto e = GetEnv(endpoint_env_var.c_str());
    if (e && !e->empty()) {
      opts.set<EndpointOption>(*std::move(e));
    }
  }
  if (!emulator_env_var.empty()) {
    auto e = GetEnv(emulator_env_var.c_str());
    if (e && !e->empty()) {
      opts.set<EndpointOption>(*std::move(e));
    }
  }

  if (!opts.has<AuthorityOption>()) {
    opts.set<AuthorityOption>(std::move(default_endpoint));
  }
  if (!authority_env_var.empty()) {
    auto e = GetEnv(authority_env_var.c_str());
    if (e && !e->empty()) {
      opts.set<AuthorityOption>(*std::move(e));
    }
  }

  auto e = GetEnv("GOOGLE_CLOUD_CPP_USER_PROJECT");
  if (e && !e->empty()) {
    opts.set<UserProjectOption>(*std::move(e));
  }

  e = GetEnv("GOOGLE_CLOUD_CPP_OPENTELEMETRY_TRACING");
  if (e && !e->empty()) {
    opts.set<OpenTelemetryTracingOption>(true);
  }
  if (!opts.has<TracingComponentsOption>()) {
    opts.set<TracingComponentsOption>(DefaultTracingComponents());
  }

  auto& products = opts.lookup<UserAgentProductsOption>();
  products.insert(products.begin(), UserAgentPrefix());

  return opts;
}

std::set<std::string> DefaultTracingComponents() {
  auto tracing = GetEnv("GOOGLE_CLOUD_CPP_ENABLE_TRACING");
  if (!tracing.has_value()) return {};
  return absl::StrSplit(*tracing, ',');
}

Options MakeAuthOptions(Options const& options) {
  Options opts;
  if (options.has<OpenTelemetryTracingOption>()) {
    opts.set<OpenTelemetryTracingOption>(
        options.get<OpenTelemetryTracingOption>());
  }
  return opts;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
