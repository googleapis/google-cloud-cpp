// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/testing/validate_metadata.h"
#include "google/cloud/status_or.h"
#include <google/api/annotations.pb.h>
#include <google/protobuf/descriptor.h>
#include <regex>

namespace grpc {
namespace testing {

/**
 * A class allowing to access internal `ClientContext` members.
 *
 * It's a hack - `ClientContext` declares such a class as a friend.
 */
class InteropClientContextInspector {
 public:
  static std::multimap<std::string, std::string> GetMetadata(
      grpc::ClientContext const& context) {
    return std::multimap<std::string, std::string>(
        context.send_initial_metadata_.begin(),
        context.send_initial_metadata_.end());
  }
};
}  // namespace testing
}  // namespace grpc

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {
namespace {

StatusOr<std::map<std::string, std::string>> ExtractMDFromHeader(
    std::string header) {
  std::map<std::string, std::string> res;
  std::regex pair_re("[^&]+");
  for (std::sregex_iterator i =
           std::sregex_iterator(header.begin(), header.end(), pair_re);
       i != std::sregex_iterator(); ++i) {
    std::regex assign_re("([^=]+)=([^=]+)");
    std::smatch match_res;
    std::string s = i->str();
    bool const matched = std::regex_match(s, match_res, assign_re);
    if (!matched) {
      return Status(
          StatusCode::kInvalidArgument,
          "Bad header format. The header should be a series of \"a=b\" "
          "delimited with \"&\", but is \"" +
              s + "\"");
    }
    bool const inserted =
        res.insert(std::make_pair(match_res[1].str(), match_res[2].str()))
            .second;
    if (!inserted) {
      return Status(
          StatusCode::kInvalidArgument,
          "Param " + match_res[1].str() + " is listed more then once");
    }
  }
  return res;
}

StatusOr<std::map<std::string, std::string>> ExtractMDFromContext(
    grpc::ClientContext const& context) {
  auto md = grpc::testing::InteropClientContextInspector::GetMetadata(context);
  auto param_header = md.equal_range("x-goog-request-params");
  if (param_header.first == param_header.second) {
    return Status(StatusCode::kInvalidArgument, "Expected header not found");
  }
  if (std::distance(param_header.first, param_header.second) > 1U) {
    return Status(StatusCode::kInvalidArgument, "Multiple headers found");
  }
  return ExtractMDFromHeader(param_header.first->second);
}

/// A poorman's check if a value matches a glob used in URL patterns.
bool ValueMatchesPattern(std::string val, std::string pattern) {
  std::string regexified_pattern =
      regex_replace(pattern, std::regex("\\*"), std::string("[^/]+"));
  return std::regex_match(val, std::regex(regexified_pattern));
}

StatusOr<std::map<std::string, std::string>> ExtractParamsFromMethod(
    std::string const& method) {
  auto method_desc =
      google::protobuf::DescriptorPool::generated_pool()->FindMethodByName(
          method);

  if (method_desc == nullptr) {
    return Status(StatusCode::kInvalidArgument,
                  "Method " + method + " is unknown.");
  }
  auto options = method_desc->options();
  if (!options.HasExtension(google::api::http)) {
    return Status(StatusCode::kInvalidArgument,
                  "Method " + method + " doesn't have a http option.");
  }
  auto const& http = options.GetExtension(google::api::http);
  std::string pattern;
  if (!http.get().empty()) {
    pattern = http.get();
  }
  if (!http.put().empty()) {
    pattern = http.put();
  }
  if (!http.post().empty()) {
    pattern = http.post();
  }
  if (!http.delete_().empty()) {
    pattern = http.delete_();
  }
  if (!http.patch().empty()) {
    pattern = http.patch();
  }
  if (http.has_custom()) {
    pattern = http.custom().path();
  }

  if (pattern.empty()) {
    return Status(
        StatusCode::kInvalidArgument,
        "Method " + method + " has a http option with an empty pattern.");
  }

  std::regex subst_re("\\{([^{}=]+)=([^{}=]+)\\}");
  std::map<std::string, std::string> res;
  for (std::sregex_iterator i =
           std::sregex_iterator(pattern.begin(), pattern.end(), subst_re);
       i != std::sregex_iterator(); ++i) {
    std::string const& param = (*i)[1].str();
    std::string const& expected_pattern = (*i)[2].str();
    res.insert(std::make_pair(param, expected_pattern));
  }
  return res;
}

}  // namespace

Status IsContextMDValid(grpc::ClientContext const& context,
                        std::string const& method) {
  auto md = ExtractMDFromContext(context);
  if (!md) {
    return md.status();
  }
  auto params = ExtractParamsFromMethod(method);
  if (!params) {
    return params.status();
  }
  for (auto const& param_pattern : *params) {
    auto const& param = param_pattern.first;
    auto const& expected_pattern = param_pattern.second;
    auto found_it = md->find(param);
    if (found_it == md->end()) {
      return Status(StatusCode::kInvalidArgument,
                    "Expected param \"" + param + "\" not found in metadata");
    }
    if (!ValueMatchesPattern(found_it->second, expected_pattern)) {
      return Status(StatusCode::kInvalidArgument,
                    "Expected param \"" + param +
                        "\" found, but its value (\"" + found_it->second +
                        "\") does not satisfy the pattern (\"" +
                        expected_pattern + "\").");
    }
  }
  return Status();
}

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
