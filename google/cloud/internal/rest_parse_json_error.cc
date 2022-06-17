// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/internal/rest_parse_json_error.h"
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

// Makes an `ErrorInfo` from an `"error"` JSON object that looks like
//   [
//     {
//       "@type": "type.googleapis.com/google.rpc.ErrorInfo",
//       "reason": "..."
//       "domain": "..."
//       "metadata": {
//         "key1": "value1"
//         ...
//       }
//     }
//   ]
// See also https://cloud.google.com/apis/design/errors#http_mapping
ErrorInfo MakeErrorInfo(int http_status_code, nlohmann::json const& details) {
  static auto constexpr kErrorInfoType =
      "type.googleapis.com/google.rpc.ErrorInfo";
  if (!details.is_array()) return ErrorInfo{};
  for (auto const& e : details.items()) {
    auto const& v = e.value();
    if (!v.is_object()) continue;
    if (!v.contains("@type") || !v["@type"].is_string()) continue;
    if (v.value("@type", "") != kErrorInfoType) continue;
    if (!v.contains("reason") || !v["reason"].is_string()) continue;
    if (!v.contains("domain") || !v["domain"].is_string()) continue;
    if (!v.contains("metadata") || !v["metadata"].is_object()) continue;
    auto const& metadata_json = v["metadata"];
    auto metadata = std::unordered_map<std::string, std::string>{};
    for (auto const& m : metadata_json.items()) {
      if (!m.value().is_string()) continue;
      metadata[m.key()] = m.value();
    }
    metadata["http_status_code"] = std::to_string(http_status_code);
    return ErrorInfo{v.value("reason", ""), v.value("domain", ""),
                     std::move(metadata)};
  }
  return ErrorInfo{};
}

}  // namespace

std::pair<std::string, ErrorInfo> ParseJsonError(int http_status_code,
                                                 std::string payload) {
  // The default result if we cannot parse the ErrorInfo.
  auto err = [&] { return std::make_pair(std::move(payload), ErrorInfo{}); };

  // We try to parse the payload as JSON, which may allow us to provide a more
  // structured and useful error Status. If the payload fails to parse as JSON,
  // we simply attach the full error payload as the Status's message string.
  auto json = nlohmann::json::parse(payload, nullptr, false);
  if (!json.is_object()) return err();

  // We expect JSON that looks like the following:
  //   {
  //     "error": {
  //       "message": "..."
  //       ...
  //       "details": [
  //         ...
  //       ]
  //     }
  //   }
  // See  https://cloud.google.com/apis/design/errors#http_mapping
  if (!json.contains("error")) return err();
  auto const& e = json["error"];
  if (!e.is_object()) return err();
  if (!e.contains("message") || !e.contains("details")) return err();
  if (!e["message"].is_string()) return err();

  return std::make_pair(e.value("message", ""),
                        MakeErrorInfo(http_status_code, e["details"]));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
