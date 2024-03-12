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

#include "generator/internal/printer.h"
#include "google/cloud/status_or.h"
#include "absl/strings/string_view.h"
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
 * Returns the namespace given a product path, and namespace type.
 *
 * Typically used with a product_path like 'google/cloud/product/v1' and
 * returns "product_v1".
 *
 * Depending on the NamespaceType, a suffix will be appended. e.g.
 * "product_v1_mocks" or "product_v1_internal".
 */
std::string Namespace(std::string const& product_path,
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
 * Change all occurrences of @p from to @p to within @p s.
 *
 * The "safe" part means it is a fatal error for @p s to already contain
 * @p to. This makes it possible to reliably reverse the mapping.
 *
 * The primary use case is to replace/restore commas in the values used by
 * ProcessCommandLineArgs(), where comma is a metacharacter (see above).
 */
std::string SafeReplaceAll(absl::string_view s, absl::string_view from,
                           absl::string_view to);

/**
 * Standard legal boilerplate file header.
 */
std::string CopyrightLicenseFileHeader();

/**
 * Current year for copyright boilerplate purposes.
 */
std::string CurrentCopyrightYear();

// Returns a copy of the input string with the first letter capitalized.
std::string CapitalizeFirstLetter(std::string str);

// Creates a formatted comment block from the provided string.
std::string FormatCommentBlock(std::string const& comment,
                               std::size_t indent_level,
                               std::string const& comment_introducer = "// ",
                               std::size_t indent_width = 2,
                               std::size_t line_length = 80);

// Creates a formatted comment block from the list of key/value pairs.
std::string FormatCommentKeyValueList(
    std::vector<std::pair<std::string, std::string>> const& comment,
    std::size_t indent_level, std::string const& separator = ":",
    std::string const& comment_introducer = "// ", std::size_t indent_width = 2,
    std::size_t line_length = 80);

// Formats a header include guard per the provided header_path.
std::string FormatHeaderIncludeGuard(absl::string_view header_path);

/// Create a directory. The parent must exist.
void MakeDirectory(std::string const& path);

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_CODEGEN_UTILS_H
