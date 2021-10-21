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
#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_CODEGEN_UTILS_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_CODEGEN_UTILS_H

#include "google/cloud/status_or.h"
#include "absl/strings/string_view.h"
#include "generator/internal/printer.h"
#include <map>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace generator_internal {

/**
 * Wraps header include in "" and returns complete include line.
 */
std::string LocalInclude(absl::string_view header);

/**
 * Wraps header include in <> and returns complete include line.
 */
std::string SystemInclude(absl::string_view header);

/**
 * Convert a CamelCase string from a protoc descriptor to snake_case.
 *
 * This function assumes inputs are correctly formatted CamelCase.
 */
std::string CamelCaseToSnakeCase(absl::string_view input);

/**
 * Convert a service name to a file path.
 *
 * service_name should consist of CamelCase pieces and "." separators.
 * Each component of service_name will become part of the path. Components will
 * be converted from CamelCase to snake_case. The trailing substring "Service"
 * will be stripped from the last component.
 *
 * Example: "google.LibraryService" -> "google/library"
 */
std::string ServiceNameToFilePath(absl::string_view service_name);

/**
 * Convert a protobuf name to a fully qualified C++ name.
 *
 * proto_name should be a "." separated name, which we convert to a
 * "::" separated C++ fully qualified name.
 */
std::string ProtoNameToCppName(absl::string_view proto_name);

enum class NamespaceType { kNormal, kInternal, kMocks };

/**
 * Builds namespace hierarchy.
 *
 * Typically used with a product_path like to 'google/cloud/product/' and
 * returns {"google", "cloud", "product", "PRODUCT_CLIENT_NS"}.
 *
 * If the path contains fewer than two directories, they will be concatenated
 * to form the product value, e.g. 'unusual/product/' returns
 * {"google", "cloud", "unusual_product", "UNUSUAL_PRODUCT_CLIENT_NS"}.
 *
 * If the path contains more than three directories the third and subsequent
 * directories will be concatenated, e.g. 'google/cloud/foo/bar/baz/' returns
 * {"google", "cloud", "foo_bar_baz", "FOO_BAR_BAZ_CLIENT_NS"}.
 *
 * If ns_type is `NamespaceType::kInternal`, "_internal" is appended to the
 * product, e.g. 'google/cloud/product/' returns
 * {"google", "cloud", "product_internal", "PRODUCT_CLIENT_NS"}.
 *
 */
std::vector<std::string> BuildNamespaces(
    std::string const& product_path,
    NamespaceType ns_type = NamespaceType::kNormal);

/**
 * Validates command line arguments passed to the microgenerator.
 *
 * Command line arguments can be passed from the protoc command line via:
 * '--cpp_codegen_opt=key=value'. This can be specified multiple times to
 * pass various key,value pairs. The resulting string passed from protoc to
 * the plugin is a comma delimited list such: "key1=value1,key2,key3=value3"
 */
StatusOr<std::vector<std::pair<std::string, std::string>>>
ProcessCommandLineArgs(std::string const& parameters);

/**
 * Standard legal boilerplate file header.
 */
std::string CopyrightLicenseFileHeader();

/**
 * Current year for copyright boilerplate purposes.
 */
std::string CurrentCopyrightYear();

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_CODEGEN_UTILS_H
