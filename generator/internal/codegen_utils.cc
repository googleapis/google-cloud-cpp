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

#include "generator/internal/codegen_utils.h"
#include "generator/internal/scaffold_generator.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/absl_str_replace_quiet.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include <google/protobuf/compiler/code_generator.h>
#include <cctype>
#include <string>
#include <unordered_set>
#if _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif  // _WIN32

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

std::vector<std::pair<std::string, std::string>> const& SnakeCaseExceptions() {
  static std::vector<std::pair<std::string, std::string>> const kExceptions = {
      {"big_query", "bigquery"}};
  return kExceptions;
}

void FormatProductPath(std::string& path) {
  if (path.front() == '/') {
    path = path.substr(1);
  }
  if (path.back() != '/') {
    path += '/';
  }
}

Status ProcessArgProductPath(
    std::vector<std::pair<std::string, std::string>>& command_line_args) {
  auto product_path =
      std::find_if(command_line_args.begin(), command_line_args.end(),
                   [](std::pair<std::string, std::string> const& p) {
                     return p.first == "product_path";
                   });
  if (product_path == command_line_args.end() || product_path->second.empty()) {
    return internal::InvalidArgumentError(
        "--cpp_codegen_opt=product_path=<path> must be specified.",
        GCP_ERROR_INFO());
  }
  FormatProductPath(product_path->second);
  return {};
}

void ProcessArgCopyrightYear(
    std::vector<std::pair<std::string, std::string>>& command_line_args) {
  auto copyright_year =
      std::find_if(command_line_args.begin(), command_line_args.end(),
                   [](std::pair<std::string, std::string> const& p) {
                     return p.first == "copyright_year";
                   });
  if (copyright_year == command_line_args.end()) {
    command_line_args.emplace_back("copyright_year", CurrentCopyrightYear());
  } else if (copyright_year->second.empty()) {
    copyright_year->second = CurrentCopyrightYear();
  }
}

void ProcessRepeated(
    std::string const& single_arg, std::string const& grouped_arg,
    std::vector<std::pair<std::string, std::string>>& command_line_args) {
  using Pair = std::pair<std::string, std::string>;
  auto it = std::partition(
      command_line_args.begin(), command_line_args.end(),
      [&single_arg](Pair const& p) { return p.first != single_arg; });
  std::unordered_set<std::string> group;
  std::transform(it, command_line_args.end(), std::inserter(group, group.end()),
                 [](Pair const& p) { return p.second; });

  command_line_args.erase(it, command_line_args.end());
  if (!group.empty()) {
    command_line_args.emplace_back(
        grouped_arg, absl::StrJoin(group.begin(), group.end(), ","));
  }
}

void ProcessArgOmitService(
    std::vector<std::pair<std::string, std::string>>& command_line_args) {
  ProcessRepeated("omit_service", "omitted_services", command_line_args);
}

void ProcessArgOmitRpc(
    std::vector<std::pair<std::string, std::string>>& command_line_args) {
  ProcessRepeated("omit_rpc", "omitted_rpcs", command_line_args);
}

void ProcessArgServiceEndpointEnvVar(
    std::vector<std::pair<std::string, std::string>>& command_line_args) {
  auto service_endpoint_env_var =
      std::find_if(command_line_args.begin(), command_line_args.end(),
                   [](std::pair<std::string, std::string> const& p) {
                     return p.first == "service_endpoint_env_var";
                   });
  if (service_endpoint_env_var == command_line_args.end()) {
    command_line_args.emplace_back("service_endpoint_env_var", "");
  }
}

void ProcessArgEmulatorEndpointEnvVar(
    std::vector<std::pair<std::string, std::string>>& command_line_args) {
  auto emulator_endpoint_env_var =
      std::find_if(command_line_args.begin(), command_line_args.end(),
                   [](std::pair<std::string, std::string> const& p) {
                     return p.first == "emulator_endpoint_env_var";
                   });
  if (emulator_endpoint_env_var == command_line_args.end()) {
    command_line_args.emplace_back("emulator_endpoint_env_var", "");
  }
}

void ProcessArgEndpointLocationStyle(
    std::vector<std::pair<std::string, std::string>>& command_line_args) {
  auto endpoint_location_style =
      std::find_if(command_line_args.begin(), command_line_args.end(),
                   [](std::pair<std::string, std::string> const& p) {
                     return p.first == "endpoint_location_style";
                   });
  if (endpoint_location_style == command_line_args.end()) {
    command_line_args.emplace_back("endpoint_location_style",
                                   "LOCATION_INDEPENDENT");
  }
}

void ProcessArgGenerateAsyncRpc(
    std::vector<std::pair<std::string, std::string>>& command_line_args) {
  ProcessRepeated("gen_async_rpc", "gen_async_rpcs", command_line_args);
}

void ProcessArgRetryGrpcStatusCode(
    std::vector<std::pair<std::string, std::string>>& command_line_args) {
  ProcessRepeated("retry_status_code", "retryable_status_codes",
                  command_line_args);
}

void ProcessArgAdditionalProtoFiles(
    std::vector<std::pair<std::string, std::string>>& command_line_args) {
  ProcessRepeated("additional_proto_file", "additional_proto_files",
                  command_line_args);
}

void ProcessArgForwardingProductPath(
    std::vector<std::pair<std::string, std::string>>& command_line_args) {
  auto path = std::find_if(command_line_args.begin(), command_line_args.end(),
                           [](std::pair<std::string, std::string> const& p) {
                             return p.first == "forwarding_product_path";
                           });
  if (path == command_line_args.end() || path->second.empty()) return;
  FormatProductPath(path->second);
}

void ProcessArgIdempotencyOverride(
    std::vector<std::pair<std::string, std::string>>& command_line_args) {
  ProcessRepeated("idempotency_override", "idempotency_overrides",
                  command_line_args);
}

void ProcessArgServiceNameMapping(
    std::vector<std::pair<std::string, std::string>>& command_line_args) {
  ProcessRepeated("service_name_mapping", "service_name_mappings",
                  command_line_args);
}

void ProcessArgServiceNameToComment(
    std::vector<std::pair<std::string, std::string>>& command_line_args) {
  ProcessRepeated("service_name_to_comment", "service_name_to_comments",
                  command_line_args);
}

}  // namespace

std::string CurrentCopyrightYear() {
  static std::string const kCurrentCopyrightYear =
      absl::FormatTime("%Y", absl::Now(), absl::UTCTimeZone());
  return kCurrentCopyrightYear;
}

std::string LocalInclude(absl::string_view header) {
  if (header.empty()) return {};
  return absl::StrCat("#include \"", header, "\"\n");
}

std::string SystemInclude(absl::string_view header) {
  if (header.empty()) return {};
  return absl::StrCat("#include <", header, ">\n");
}

std::string CamelCaseToSnakeCase(absl::string_view input) {
  std::string output;
  for (auto i = 0U; i < input.size(); ++i) {
    auto const uc = static_cast<unsigned char>(input[i]);
    auto const lower = static_cast<char>(std::tolower(uc));
    if (input[i] != '_' && i + 2 < input.size()) {
      if (std::isupper(static_cast<unsigned char>(input[i + 1])) &&
          std::islower(static_cast<unsigned char>(input[i + 2]))) {
        output += lower;
        output += '_';
        continue;
      }
    }
    if (input[i] != '_' && i + 1 < input.size()) {
      if ((std::islower(static_cast<unsigned char>(input[i])) ||
           std::isdigit(static_cast<unsigned char>(input[i]))) &&
          std::isupper(static_cast<unsigned char>(input[i + 1]))) {
        output += lower;
        output += '_';
        continue;
      }
    }
    output += lower;
  }

  output = absl::StrReplaceAll(output, SnakeCaseExceptions());
  return output;
}

std::string ServiceNameToFilePath(absl::string_view service_name) {
  std::vector<absl::string_view> components = absl::StrSplit(service_name, '.');
  absl::ConsumeSuffix(&components.back(), "Service");
  auto formatter = [](std::string* s, absl::string_view sv) {
    *s += CamelCaseToSnakeCase(sv);
  };
  return absl::StrJoin(components, "/", formatter);
}

std::string ProtoNameToCppName(absl::string_view proto_name) {
  return absl::StrReplaceAll(proto_name, {{".", "::"}});
}

std::string Namespace(std::string const& product_path, NamespaceType ns_type) {
  auto ns = absl::StrCat(LibraryName(product_path), "/",
                         ServiceSubdirectory(product_path));
  ns = std::string{absl::StripSuffix(ns, "/")};
  ns = absl::StrReplaceAll(ns, {{"/", "_"}});

  if (ns_type == NamespaceType::kInternal) {
    absl::StrAppend(&ns, "_internal");
  }
  if (ns_type == NamespaceType::kMocks) {
    absl::StrAppend(&ns, "_mocks");
  }
  return ns;
}

StatusOr<std::vector<std::pair<std::string, std::string>>>
ProcessCommandLineArgs(std::string const& parameters) {
  std::vector<std::pair<std::string, std::string>> command_line_args;
  google::protobuf::compiler::ParseGeneratorParameter(parameters,
                                                      &command_line_args);
  auto status = ProcessArgProductPath(command_line_args);
  if (!status.ok()) return status;

  ProcessArgCopyrightYear(command_line_args);
  ProcessArgOmitService(command_line_args);
  ProcessArgOmitRpc(command_line_args);
  ProcessArgServiceEndpointEnvVar(command_line_args);
  ProcessArgEmulatorEndpointEnvVar(command_line_args);
  ProcessArgEndpointLocationStyle(command_line_args);
  ProcessArgGenerateAsyncRpc(command_line_args);
  ProcessArgRetryGrpcStatusCode(command_line_args);
  ProcessArgAdditionalProtoFiles(command_line_args);
  ProcessArgForwardingProductPath(command_line_args);
  ProcessArgIdempotencyOverride(command_line_args);
  ProcessArgServiceNameMapping(command_line_args);
  ProcessArgServiceNameToComment(command_line_args);
  return command_line_args;
}

std::string SafeReplaceAll(absl::string_view s, absl::string_view from,
                           absl::string_view to) {
  if (absl::StrContains(s, to)) {
    GCP_LOG(FATAL) << "SafeReplaceAll() found \"" << to << "\" in \"" << s
                   << "\"";
  }
  return absl::StrReplaceAll(s, {{from, to}});
}

std::string CopyrightLicenseFileHeader() {
  static auto constexpr kHeader =  // clang-format off
  "// Copyright $copyright_year$ Google LLC\n"
  "//\n"
  "// Licensed under the Apache License, Version 2.0 (the \"License\");\n"
  "// you may not use this file except in compliance with the License.\n"
  "// You may obtain a copy of the License at\n"
  "//\n"
  "//      https://www.apache.org/licenses/LICENSE-2.0\n"
  "//\n"
  "// Unless required by applicable law or agreed to in writing, software\n"
  "// distributed under the License is distributed on an \"AS IS\" BASIS,\n"
  "// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n"
  "// See the License for the specific language governing permissions and\n"
  "// limitations under the License.\n";
  // clang-format on
  return kHeader;
}

std::string CapitalizeFirstLetter(std::string str) {
  str[0] = static_cast<unsigned char>(
      std::toupper(static_cast<unsigned char>(str[0])));
  return str;
}

std::string FormatCommentBlock(std::string const& comment,
                               std::size_t indent_level,
                               std::string const& comment_introducer,
                               std::size_t indent_width,
                               std::size_t line_length) {
  if (comment.empty()) return {};
  auto offset = indent_level * indent_width + comment_introducer.length();
  if (offset >= line_length) GCP_LOG(FATAL) << "line_length is too small";
  auto comment_width = line_length - offset;

  std::vector<std::string> lines;
  std::size_t start_pos = 0;
  while (start_pos != std::string::npos) {
    std::size_t boundary = start_pos + comment_width;
    std::size_t end_pos = boundary;
    if (boundary < comment.length()) {
      // Look backward from the boundary for the last word
      end_pos = comment.rfind(' ', boundary);
      // If there is only one word, find and use its boundary
      if (end_pos == std::string::npos || end_pos < start_pos) {
        end_pos = comment.find(' ', boundary);
      }
    }
    lines.push_back(comment.substr(start_pos, end_pos - start_pos));
    start_pos = comment.find_first_not_of(' ', end_pos);
  }

  std::string indent(indent_level * indent_width, ' ');
  std::string joiner = absl::StrCat("\n", indent, comment_introducer);
  return absl::StrCat(indent, comment_introducer, absl::StrJoin(lines, joiner));
}

std::string FormatCommentKeyValueList(
    std::vector<std::pair<std::string, std::string>> const& comment,
    std::size_t indent_level, std::string const& separator,
    std::string const& comment_introducer, std::size_t indent_width,
    std::size_t line_length) {
  if (comment.empty() || line_length == 0) return {};
  auto formatter = [&](std::string* s,
                       std::pair<std::string, std::string> const& p) {
    auto raw = absl::StrCat(p.first, separator, " ", p.second);
    auto formatted = FormatCommentBlock(raw, indent_level, comment_introducer,
                                        indent_width, line_length);
    *s += formatted;
  };

  return absl::StrCat(absl::StrJoin(comment, "\n", formatter));
}

std::string FormatHeaderIncludeGuard(absl::string_view header_path) {
  return absl::AsciiStrToUpper(
      absl::StrReplaceAll(absl::StrCat("GOOGLE_CLOUD_CPP_", header_path),
                          {{"/", "_"}, {".", "_"}}));
}

void MakeDirectory(std::string const& path) {
#if _WIN32
  _mkdir(path.c_str());
#else
  if (mkdir(path.c_str(), 0755) == 0) return;
  if (errno != EEXIST) {
    GCP_LOG(ERROR) << "Unable to create directory for path:" << path << "\n";
  }
#endif  // _WIN32
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
