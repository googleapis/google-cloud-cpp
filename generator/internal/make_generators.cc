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
#include "generator/internal/mock_connection_generator.h"
#include "generator/internal/option_defaults_generator.h"
#include "generator/internal/options_generator.h"
#include "generator/internal/printer.h"
#include "generator/internal/retry_traits_generator.h"
#include "generator/internal/round_robin_decorator_generator.h"
#include "generator/internal/sample_generator.h"
#include "generator/internal/stub_factory_generator.h"
#include "generator/internal/stub_factory_rest_generator.h"
#include "generator/internal/stub_generator.h"
#include "generator/internal/stub_rest_generator.h"
#include "generator/internal/tracing_connection_generator.h"
#include "generator/internal/tracing_stub_generator.h"
#include <google/protobuf/compiler/code_generator.h>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace generator_internal {

std::vector<std::unique_ptr<GeneratorInterface>> MakeGenerators(
    google::protobuf::ServiceDescriptor const* service,
    google::protobuf::compiler::GeneratorContext* context,
    std::vector<std::pair<std::string, std::string>> const& vars) {
  std::vector<std::unique_ptr<GeneratorInterface>> code_generators;
  VarsDictionary service_vars = CreateServiceVars(*service, vars);
  auto method_vars = CreateMethodVars(*service, service_vars);
  bool const generate_grpc_transport = [&] {
    auto iter = service_vars.find("generate_grpc_transport");
    if (iter == service_vars.end()) return true;
    return iter->second == "true";
  }();

  auto const omit_client = service_vars.find("omit_client");
  if (omit_client == service_vars.end() || omit_client->second != "true") {
    code_generators.push_back(std::make_unique<ClientGenerator>(
        service, service_vars, method_vars, context));
    code_generators.push_back(std::make_unique<SampleGenerator>(
        service, service_vars, method_vars, context));
  }
  auto const omit_connection = service_vars.find("omit_connection");
  if (omit_connection == service_vars.end() ||
      omit_connection->second != "true") {
    if (generate_grpc_transport) {
      code_generators.push_back(std::make_unique<ConnectionImplGenerator>(
          service, service_vars, method_vars, context));
    }
    code_generators.push_back(std::make_unique<ConnectionGenerator>(
        service, service_vars, method_vars, context));
    code_generators.push_back(std::make_unique<IdempotencyPolicyGenerator>(
        service, service_vars, method_vars, context));
    code_generators.push_back(std::make_unique<MockConnectionGenerator>(
        service, service_vars, method_vars, context));
    code_generators.push_back(std::make_unique<OptionDefaultsGenerator>(
        service, service_vars, method_vars, context));
    code_generators.push_back(std::make_unique<OptionsGenerator>(
        service, service_vars, method_vars, context));
    if (service_vars.find("retry_status_code_expression") !=
        service_vars.end()) {
      code_generators.push_back(std::make_unique<RetryTraitsGenerator>(
          service, service_vars, method_vars, context));
    }
    code_generators.push_back(std::make_unique<TracingConnectionGenerator>(
        service, service_vars, method_vars, context));
  }
  auto const omit_stub_factory = service_vars.find("omit_stub_factory");
  if (omit_stub_factory == service_vars.end() ||
      omit_stub_factory->second != "true") {
    if (generate_grpc_transport) {
      code_generators.push_back(std::make_unique<StubFactoryGenerator>(
          service, service_vars, method_vars, context));
    }
  }
  auto const forwarding_headers = service_vars.find("forwarding_product_path");
  if (forwarding_headers != service_vars.end() &&
      !forwarding_headers->second.empty()) {
    code_generators.push_back(std::make_unique<ForwardingClientGenerator>(
        service, service_vars, method_vars, context));
    code_generators.push_back(std::make_unique<ForwardingConnectionGenerator>(
        service, service_vars, method_vars, context));
    code_generators.push_back(
        std::make_unique<ForwardingIdempotencyPolicyGenerator>(
            service, service_vars, method_vars, context));
    code_generators.push_back(
        std::make_unique<ForwardingMockConnectionGenerator>(
            service, service_vars, method_vars, context));
    code_generators.push_back(std::make_unique<ForwardingOptionsGenerator>(
        service, service_vars, method_vars, context));
  }

  if (generate_grpc_transport) {
    code_generators.push_back(std::make_unique<AuthDecoratorGenerator>(
        service, service_vars, method_vars, context));
    code_generators.push_back(std::make_unique<LoggingDecoratorGenerator>(
        service, service_vars, method_vars, context));
    code_generators.push_back(std::make_unique<MetadataDecoratorGenerator>(
        service, service_vars, method_vars, context));
    code_generators.push_back(std::make_unique<StubGenerator>(
        service, service_vars, method_vars, context));
    code_generators.push_back(std::make_unique<TracingStubGenerator>(
        service, service_vars, method_vars, context));
  }

  auto const generate_round_robin_generator =
      service_vars.find("generate_round_robin_decorator");
  if (generate_round_robin_generator != service_vars.end() &&
      generate_round_robin_generator->second == "true") {
    code_generators.push_back(std::make_unique<RoundRobinDecoratorGenerator>(
        service, service_vars, method_vars, context));
  }

  auto const generate_rest_transport =
      service_vars.find("generate_rest_transport");
  if (generate_rest_transport != service_vars.end() &&
      generate_rest_transport->second == "true") {
    // All REST interfaces postdate the change to the format of our inline
    // namespace name, so we never need to add a backwards-compatibility alias.
    auto rest_service_vars = service_vars;
    rest_service_vars.erase("backwards_compatibility_namespace_alias");
    code_generators.push_back(std::make_unique<ConnectionRestGenerator>(
        service, rest_service_vars, method_vars, context));
    code_generators.push_back(std::make_unique<ConnectionImplRestGenerator>(
        service, rest_service_vars, method_vars, context));
    code_generators.push_back(std::make_unique<LoggingDecoratorRestGenerator>(
        service, rest_service_vars, method_vars, context));
    code_generators.push_back(std::make_unique<MetadataDecoratorRestGenerator>(
        service, rest_service_vars, method_vars, context));
    code_generators.push_back(std::make_unique<StubFactoryRestGenerator>(
        service, rest_service_vars, method_vars, context));
    code_generators.push_back(std::make_unique<StubRestGenerator>(
        service, rest_service_vars, method_vars, context));
  }

  return code_generators;
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
