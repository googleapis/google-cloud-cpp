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

#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_HTTP_OPTION_UTILS_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_HTTP_OPTION_UTILS_H

#include "generator/internal/http_annotation_parser.h"
#include "generator/internal/printer.h"
#include "absl/types/variant.h"
#include <google/protobuf/descriptor.h>
#include <string>

namespace google {
namespace cloud {
namespace generator_internal {

struct HttpSimpleInfo {
  std::string http_verb;
  std::string url_path;
  std::string body;
  std::string api_version;
};

struct HttpExtensionInfo {
  using RestPathPiece = std::function<std::string(
      google::protobuf::MethodDescriptor const&, bool)>;
  std::string http_verb;
  std::string url_path;
  std::vector<std::pair<std::string, std::string>> field_substitutions;
  std::string body;
  std::vector<RestPathPiece> rest_path;
  std::string rest_path_verb;
};

/**
 * Parses the http extension providing resource routing info, if present,
 * for the provided method per AIP-4222. Output is also used for gRPC/HTTP
 * transcoding and REST transport.
 */
HttpExtensionInfo ParseHttpExtension(
    google::protobuf::MethodDescriptor const& method);

/**
 * Sets the following method_vars based on the provided parsed_http_info:
 *   method_request_url_path
 *   method_request_url_substitution
 *   method_request_param_key
 *   method_request_param_value
 *   method_request_body
 *   method_http_verb
 *   method_rest_path
 */
void SetHttpDerivedMethodVars(
    absl::variant<absl::monostate, HttpSimpleInfo, HttpExtensionInfo>
        parsed_http_info,
    google::protobuf::MethodDescriptor const& method,
    VarsDictionary& method_vars);

struct QueryParameterInfo {
  protobuf::FieldDescriptor::CppType cpp_type;
  // A code fragment the generator emits to access the value of the field.
  std::string request_field_accessor;
  // Check presence for MESSAGE types as their default values may result in
  // undesired behavior.
  bool check_presence;
};

/**
 * Determine if a field is a query parameter candidate, such that it's a
 * non-repeated field that is also not an aggregate type. This includes numeric,
 * bool, and string native protobuf data types, as well as, protobuf "Well Known
 * Types" that wrap those data types.
 */
absl::optional<QueryParameterInfo> DetermineQueryParameterInfo(
    google::protobuf::FieldDescriptor const& field);

/**
 * Sets the "method_http_query_parameters" value in method_vars based on the
 * parsed_http_info.
 */
void SetHttpQueryParameters(
    absl::variant<absl::monostate, HttpSimpleInfo, HttpExtensionInfo>
        parsed_http_info,
    google::protobuf::MethodDescriptor const& method,
    VarsDictionary& method_vars);

/**
 * Determines if the method contains a routing header as specified in AIP-4222.
 */
bool HasHttpRoutingHeader(google::protobuf::MethodDescriptor const& method);

/**
 * Determines if the method contains a google.api.http annotation necessary for
 * supporting REST transport.
 */
bool HasHttpAnnotation(google::protobuf::MethodDescriptor const& method);

/**
 * Determines the value of the "request_resource" value in method_vars.
 *
 * If the rpc has the google.api.http extension with the body field specified as
 * not "*", that field is returned, else if the rpc request message has a field
 * annotated with:
 *  [json_name = __json_request_body] that field is returned. Otherwise, the
 *  entire request is used.
 */
std::string FormatRequestResource(google::protobuf::Descriptor const& request,
                                  HttpExtensionInfo const& info);

/**
 * Parses the package name of the method and returns its API version.
 */
std::string FormatApiVersionFromPackageName(
    google::protobuf::MethodDescriptor const& method);

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_HTTP_OPTION_UTILS_H
