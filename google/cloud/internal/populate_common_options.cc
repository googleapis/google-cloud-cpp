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
#include "google/cloud/credentials.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/service_endpoint.h"
#include "google/cloud/internal/user_agent_prefix.h"
#include "google/cloud/opentelemetry_options.h"
#include "google/cloud/universe_domain_options.h"
#include "absl/strings/str_split.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

Options PopulateCommonOptions(Options opts, std::string const& endpoint_env_var,
                              std::string const& emulator_env_var,
                              std::string const& authority_env_var,
                              std::string default_endpoint) {
  auto e = GetEnv("GOOGLE_CLOUD_UNIVERSE_DOMAIN");
  if (e && !e->empty()) opts.set<UniverseDomainOption>(*std::move(e));
  default_endpoint = UniverseDomainEndpoint(std::move(default_endpoint), opts);
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
      opts.set<UnifiedCredentialsOption>(MakeInsecureCredentials());
    }
  }
  if (!opts.has<EndpointOption>()) {
    opts.set<EndpointOption>(default_endpoint);
  }

  if (!authority_env_var.empty()) {
    auto e = GetEnv(authority_env_var.c_str());
    if (e && !e->empty()) {
      opts.set<AuthorityOption>(*std::move(e));
    }
  }
  if (!opts.has<AuthorityOption>()) {
    opts.set<AuthorityOption>(std::move(default_endpoint));
  }

  e = GetEnv("GOOGLE_CLOUD_CPP_USER_PROJECT");
  if (!e || e->empty()) e = GetEnv("GOOGLE_CLOUD_QUOTA_PROJECT");
  if (e && !e->empty()) {
    opts.set<UserProjectOption>(*std::move(e));
  }

  e = GetEnv("GOOGLE_CLOUD_CPP_OPENTELEMETRY_TRACING");
  if (e && !e->empty()) {
    opts.set<OpenTelemetryTracingOption>(true);
  }
  if (!opts.has<LoggingComponentsOption>()) {
    opts.set<LoggingComponentsOption>(DefaultTracingComponents());
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

TracingOptions DefaultTracingOptions() {
  auto tracing_options = GetEnv("GOOGLE_CLOUD_CPP_TRACING_OPTIONS");
  if (!tracing_options) return {};
  return TracingOptions{}.SetOptions(*tracing_options);
}

// TODO(#15089): Determine if this function needs to preserve more (or all) of
// the options passed in.
Options MakeAuthOptions(Options const& options) {
  Options opts;
  if (options.has<OpenTelemetryTracingOption>()) {
    opts.set<OpenTelemetryTracingOption>(
        options.get<OpenTelemetryTracingOption>());
  }
  if (options.has<LoggingComponentsOption>()) {
    opts.set<LoggingComponentsOption>(options.get<LoggingComponentsOption>());
  }
  return opts;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
