// Copyright 2020 Google LLC
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
#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_DESCRIPTOR_UTILS_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_DESCRIPTOR_UTILS_H

#include "generator/internal/generator_interface.h"
#include "generator/internal/predicate_utils.h"
#include "generator/internal/printer.h"
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/descriptor.h>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace generator_internal {

/**
 * Extracts service wide substitution data required by all class generators from
 * the provided descriptor.
 */
VarsDictionary CreateServiceVars(
    google::protobuf::ServiceDescriptor const& descriptor,
    std::vector<std::pair<std::string, std::string>> const& initial_values);

/**
 * Extracts method specific substitution data for each method in the service.
 */
std::map<std::string, VarsDictionary> CreateMethodVars(
    google::protobuf::ServiceDescriptor const& service,
    VarsDictionary const& service_vars);

/**
 * Creates and initializes the collection of ClassGenerators necessary to
 * generate all code for the given service.
 */
std::vector<std::unique_ptr<GeneratorInterface>> MakeGenerators(
    google::protobuf::ServiceDescriptor const* service,
    google::protobuf::compiler::GeneratorContext* context,
    std::vector<std::pair<std::string, std::string>> const& vars);

/**
 * Determines which `MethodPattern` to use from patterns for the given method
 * and invokes the provided printer with the `PredicatedFragment`s in patterns
 * with the substitution data in vars.
 *
 * Exactly one `MethodPattern` in patters should match the method. If none or
 * more than one match, an error is returned.
 *
 * file and line are used to provide better diagnostic messages in case of a
 * substitution error.
 *
 */
Status PrintMethod(google::protobuf::MethodDescriptor const& method,
                   Printer& printer, VarsDictionary const& vars,
                   std::vector<MethodPattern> const& patterns, char const* file,
                   int line);

enum class MethodParameterStyle { kApiMethodSignature, kProtobufReqeust };
/**
 * Formats comments from the source .proto file into Doxygen compatible
 * function headers, including param and return lines as necessary.
 */
std::string FormatMethodCommentsFromRpcComments(
    google::protobuf::MethodDescriptor const& method,
    MethodParameterStyle parameter_style);

struct ResourceRoutingInfo {
  std::string url_path;
  std::string param_key;
  std::string url_substituion;
  std::string body;
};
/**
 * Parses the resource routing info, if present, for the provided method
 * per AIP-4222.
 */
absl::optional<ResourceRoutingInfo> ParseResourceRoutingHeader(
    google::protobuf::MethodDescriptor const& method);

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_DESCRIPTOR_UTILS_H
