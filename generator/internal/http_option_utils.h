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
};

struct HttpExtensionInfo {
  using RestPathPiece =
      std::function<std::string(google::protobuf::MethodDescriptor const&)>;
  std::string http_verb;
  std::string url_path;
  std::string request_field_name;
  std::string url_substitution;
  std::string body;
  std::string path_prefix;
  std::string path_suffix;
  std::vector<RestPathPiece> rest_path;
};

/**
 * Parses the http extension providing resource routing info, if present,
 * for the provided method per AIP-4222. Output is also used for gRPC/HTTP
 * transcoding and REST transport.
 */
absl::variant<absl::monostate, HttpSimpleInfo, HttpExtensionInfo>
ParseHttpExtension(google::protobuf::MethodDescriptor const& method);

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

/**
 * Sets the "method_http_query_parameters" value in method_vars based on the
 * parsed_http_info.
 */
void SetHttpGetQueryParameters(
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

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_HTTP_OPTION_UTILS_H
