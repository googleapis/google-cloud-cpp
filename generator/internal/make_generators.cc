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

#include "generator/internal/make_generators.h"
#include "generator/internal/auth_decorator_generator.h"
#include "generator/internal/client_generator.h"
#include "generator/internal/connection_generator.h"
#include "generator/internal/connection_impl_generator.h"
#include "generator/internal/connection_impl_rest_generator.h"
#include "generator/internal/connection_rest_generator.h"
#include "generator/internal/descriptor_utils.h"
#include "generator/internal/forwarding_client_generator.h"
#include "generator/internal/forwarding_connection_generator.h"
#include "generator/internal/forwarding_idempotency_policy_generator.h"
#include "generator/internal/forwarding_mock_connection_generator.h"
#include "generator/internal/forwarding_options_generator.h"
#include "generator/internal/idempotency_policy_generator.h"
#include "generator/internal/logging_decorator_generator.h"
#include "generator/internal/logging_decorator_rest_generator.h"
#include "generator/internal/metadata_decorator_generator.h"
#include "generator/internal/metadata_decorator_rest_generator.h"
#include "generator/internal/mixin_utils.h"
#include "generator/internal/mock_connection_generator.h"
#include "generator/internal/option_defaults_generator.h"
#include "generator/internal/options_generator.h"
#include "generator/internal/printer.h"
#include "generator/internal/retry_traits_generator.h"
#include "generator/internal/round_robin_decorator_generator.h"
#include "generator/internal/sample_generator.h"
#include "generator/internal/sources_generator.h"
#include "generator/internal/stub_factory_generator.h"
#include "generator/internal/stub_factory_rest_generator.h"
#include "generator/internal/stub_generator.h"
#include "generator/internal/stub_rest_generator.h"
#include "generator/internal/tracing_connection_generator.h"
#include "generator/internal/tracing_stub_generator.h"
#include <google/protobuf/compiler/code_generator.h>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace generator_internal {

std::vector<std::unique_ptr<GeneratorInterface>> MakeGenerators(
    google::protobuf::ServiceDescriptor const* service,
    google::protobuf::compiler::GeneratorContext* context,
    YAML::Node const& service_config,
    std::vector<std::pair<std::string, std::string>> const& vars) {
  std::vector<MixinMethod> mixin_methods;
  if (service->file()->name() == "google/pubsub/v1/pubsub.proto" ||
      service->name() == "DataMigrationService") {
    mixin_methods = GetMixinMethods(service_config, *service);
  }
  std::vector<std::string> sources;
  std::vector<std::unique_ptr<GeneratorInterface>> code_generators;
  VarsDictionary service_vars =
      CreateServiceVars(*service, vars, mixin_methods);
  auto method_vars =
      CreateMethodVars(*service, service_config, service_vars, mixin_methods);
  auto get_flag = [&](std::string const& key, bool default_value = false) {
    auto iter = service_vars.find(key);
    if (iter == service_vars.end()) return default_value;
    return iter->second == "true";
  };
  bool const generate_grpc_transport =
      get_flag("generate_grpc_transport", true);
  auto const generate_rest_transport = get_flag("generate_rest_transport");
  auto const omit_client = get_flag("omit_client");

  if (!omit_client) {
    code_generators.push_back(std::make_unique<ClientGenerator>(
        service, service_vars, method_vars, context, mixin_methods));
    code_generators.push_back(std::make_unique<SampleGenerator>(
        service, service_vars, method_vars, context, mixin_methods));
  }
  if (!get_flag("omit_connection")) {
    if (generate_grpc_transport) {
      code_generators.push_back(std::make_unique<ConnectionImplGenerator>(
          service, service_vars, method_vars, context, mixin_methods));
    }
    code_generators.push_back(std::make_unique<ConnectionGenerator>(
        service, service_vars, method_vars, context, mixin_methods));
    code_generators.push_back(std::make_unique<IdempotencyPolicyGenerator>(
        service, service_vars, method_vars, context, mixin_methods));
    code_generators.push_back(std::make_unique<MockConnectionGenerator>(
        service, service_vars, method_vars, context, mixin_methods));
    code_generators.push_back(std::make_unique<OptionDefaultsGenerator>(
        service, service_vars, method_vars, context, mixin_methods));
    code_generators.push_back(std::make_unique<OptionsGenerator>(
        service, service_vars, method_vars, context, mixin_methods));
    if (service_vars.find("retry_status_code_expression") !=
        service_vars.end()) {
      code_generators.push_back(std::make_unique<RetryTraitsGenerator>(
          service, service_vars, method_vars, context, mixin_methods));
    }
    code_generators.push_back(std::make_unique<TracingConnectionGenerator>(
        service, service_vars, method_vars, context, mixin_methods));
  }
  if (!get_flag("omit_stub_factory") && generate_grpc_transport) {
    code_generators.push_back(std::make_unique<StubFactoryGenerator>(
        service, service_vars, method_vars, context, mixin_methods));
  }
  auto const forwarding_headers = service_vars.find("forwarding_product_path");
  if (forwarding_headers != service_vars.end() &&
      !forwarding_headers->second.empty()) {
    code_generators.push_back(std::make_unique<ForwardingClientGenerator>(
        service, service_vars, method_vars, context, mixin_methods));
    code_generators.push_back(std::make_unique<ForwardingConnectionGenerator>(
        service, service_vars, method_vars, context, mixin_methods));
    code_generators.push_back(
        std::make_unique<ForwardingIdempotencyPolicyGenerator>(
            service, service_vars, method_vars, context, mixin_methods));
    code_generators.push_back(
        std::make_unique<ForwardingMockConnectionGenerator>(
            service, service_vars, method_vars, context, mixin_methods));
    code_generators.push_back(std::make_unique<ForwardingOptionsGenerator>(
        service, service_vars, method_vars, context, mixin_methods));
  }

  if (generate_grpc_transport) {
    code_generators.push_back(std::make_unique<AuthDecoratorGenerator>(
        service, service_vars, method_vars, context, mixin_methods));
    code_generators.push_back(std::make_unique<LoggingDecoratorGenerator>(
        service, service_vars, method_vars, context, mixin_methods));
    code_generators.push_back(std::make_unique<MetadataDecoratorGenerator>(
        service, service_vars, method_vars, context, mixin_methods));
    code_generators.push_back(std::make_unique<StubGenerator>(
        service, service_vars, method_vars, context, mixin_methods));
    code_generators.push_back(std::make_unique<TracingStubGenerator>(
        service, service_vars, method_vars, context, mixin_methods));
  }

  if (get_flag("generate_round_robin_decorator")) {
    code_generators.push_back(std::make_unique<RoundRobinDecoratorGenerator>(
        service, service_vars, method_vars, context, mixin_methods));
    sources.push_back(service_vars["round_robin_cc_path"]);
  }

  if (generate_rest_transport) {
    // All REST interfaces postdate the change to the format of our inline
    // namespace name, so we never need to add a backwards-compatibility alias.
    auto rest_service_vars = service_vars;
    rest_service_vars.erase("backwards_compatibility_namespace_alias");
    code_generators.push_back(std::make_unique<ConnectionRestGenerator>(
        service, rest_service_vars, method_vars, context, mixin_methods));
    code_generators.push_back(std::make_unique<ConnectionImplRestGenerator>(
        service, rest_service_vars, method_vars, context, mixin_methods));
    code_generators.push_back(std::make_unique<LoggingDecoratorRestGenerator>(
        service, rest_service_vars, method_vars, context, mixin_methods));
    code_generators.push_back(std::make_unique<MetadataDecoratorRestGenerator>(
        service, rest_service_vars, method_vars, context, mixin_methods));
    code_generators.push_back(std::make_unique<StubFactoryRestGenerator>(
        service, rest_service_vars, method_vars, context, mixin_methods));
    code_generators.push_back(std::make_unique<StubRestGenerator>(
        service, rest_service_vars, method_vars, context, mixin_methods));
  }

  if (!omit_client) {
    // Only use `SourcesGenerator` for fully generated libraries. If we have a
    // handwritten client for a service, we should handwrite the conglomerate
    // sources file.
    sources.push_back(service_vars["client_cc_path"]);
    sources.push_back(service_vars["connection_cc_path"]);
    sources.push_back(service_vars["idempotency_policy_cc_path"]);
    sources.push_back(service_vars["option_defaults_cc_path"]);
    sources.push_back(service_vars["tracing_connection_cc_path"]);

    if (generate_grpc_transport) {
      sources.push_back(service_vars["connection_impl_cc_path"]);
      sources.push_back(service_vars["stub_factory_cc_path"]);
      sources.push_back(service_vars["auth_cc_path"]);
      sources.push_back(service_vars["logging_cc_path"]);
      sources.push_back(service_vars["metadata_cc_path"]);
      sources.push_back(service_vars["stub_cc_path"]);
      sources.push_back(service_vars["tracing_stub_cc_path"]);
      if (get_flag("omit_streaming_updater")) {
        sources.push_back(service_vars["streaming_cc_path"]);
      }
    }

    if (generate_rest_transport) {
      sources.push_back(service_vars["connection_rest_cc_path"]);
      sources.push_back(service_vars["connection_impl_rest_cc_path"]);
      sources.push_back(service_vars["logging_rest_cc_path"]);
      sources.push_back(service_vars["metadata_rest_cc_path"]);
      sources.push_back(service_vars["stub_factory_rest_cc_path"]);
      sources.push_back(service_vars["stub_rest_cc_path"]);
    }

    std::sort(sources.begin(), sources.end());
    code_generators.push_back(std::make_unique<SourcesGenerator>(
        service, service_vars, method_vars, context, std::move(sources),
        mixin_methods));
  }

  return code_generators;
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
