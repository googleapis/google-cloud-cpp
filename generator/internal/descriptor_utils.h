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

#include "absl/types/variant.h"
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

/**
 * Formats comments from the source .proto file into Doxygen compatible
 * function headers, including param and return lines as necessary.
 */
std::string FormatMethodComments(
    google::protobuf::MethodDescriptor const& method,
    std::string const& variable_parameter_comments);

/**
 * Formats comments from the source .proto file into Doxygen compatible
 * function headers, including param and return lines as necessary, for a client
 * call with the given method signature overload.
 */
std::string FormatMethodCommentsMethodSignature(
    google::protobuf::MethodDescriptor const& method,
    std::string const& signature);

/**
 * Formats comments from the source .proto file into Doxygen compatible
 * function headers, including param and return lines as necessary, for a client
 * call using the raw request proto type as input.
 */
std::string FormatMethodCommentsProtobufRequest(
    google::protobuf::MethodDescriptor const& method);

struct HttpSimpleInfo {
  std::string http_verb;
  std::string url_path;
  std::string body;
};

struct HttpExtensionInfo {
  std::string http_verb;
  std::string url_path;
  std::string request_field_name;
  std::string url_substitution;
  std::string body;
  std::string path_prefix;
  std::string path_suffix;
};

/**
 * Parses the http extension providing resource routing info, if present,
 * for the provided method per AIP-4222. Output is also used for gRPC/HTTP
 * transcoding.
 */
absl::variant<absl::monostate, HttpSimpleInfo, HttpExtensionInfo>
ParseHttpExtension(google::protobuf::MethodDescriptor const& method);

/// Our representation for a `google.api.RoutingParameter` message.
struct RoutingParameter {
  /**
   * A processed `field` string from a `RoutingParameter` proto.
   *
   * It has potentially been modified to avoid naming conflicts (e.g. a proto
   * field named "namespace" will be stored here as "namespace_").
   *
   * We will generate code like: `"request." + field_name + "();"` in our
   * metadata decorator to access the field's value.
   */
  std::string field_name;

  /**
   * A processed `path_template` string from a `RoutingParameter` proto.
   *
   * It is translated for use as a `std::regex`. We make the following
   * substitutions, simultaneously:
   *   - "**"    => ".*"
   *   - "*"     => "[^/]+"
   *   - "{foo=" => "("
   *   - "}"     => ")"
   *
   * Note that we do not store the routing parameter key ("foo" in this example)
   * in this struct. It is instead stored as a key in the `ExplicitRoutingInfo`
   * map.
   */
  std::string pattern;
};

/**
 * A data structure to represent the logic of a `google.api.RoutingRule`, in a
 * form that facilitates code generation.
 *
 * The keys of the map are the extracted routing param keys. They map to an
 * ordered list of matching rules. For this object, the first match will win.
 */
using ExplicitRoutingInfo =
    std::unordered_map<std::string, std::vector<RoutingParameter>>;

/**
 * Parses the explicit resource routing info as defined in the
 * google.api.routing annotation.
 *
 * This method processes the `google.api.RoutingRule` proto. It groups the
 * `google.api.RoutingParameters` by the extracted routing parameter key.
 *
 * Each `google.api.RoutingParameter` message is translated into a form that is
 * easier for our generator to work with (see `RoutingParameter` above).
 *
 * We reverse the order of the `RoutingParameter`s. The rule (as defined in
 * go/actools-dynamic-routing-proposal) is that "last wins". We would like to
 * order them such that "first wins", so we can stop iterating when we have
 * found a match.
 */
ExplicitRoutingInfo ParseExplicitRoutingHeader(
    google::protobuf::MethodDescriptor const& method);

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_DESCRIPTOR_UTILS_H
