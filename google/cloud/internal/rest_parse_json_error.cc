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
//       "reason": "..."
//       "domain": "..."
//       "metadata": {
//         "key1": "value1"
//         ...
//       }
//     }
//   ]
// See also https://cloud.google.com/apis/design/errors#http_mapping
//
// If there is a '@type' field then its value must be
//     type.googleapis.com/google.rpc.ErrorInfo
//
// The metadata field may be absent.
ErrorInfo MakeErrorInfo(int http_status_code, nlohmann::json const& details) {
  static auto constexpr kErrorInfoType =
      "type.googleapis.com/google.rpc.ErrorInfo";
  if (!details.is_array()) return ErrorInfo{};
  for (auto const& e : details.items()) {
    auto const& v = e.value();
    if (!v.is_object()) continue;
    auto ty = v.find("@type");
    if (ty != v.end() && ty->is_string() &&
        ty->get<std::string>() != kErrorInfoType) {
      continue;
    }
    auto r = v.find("reason");
    if (r == v.end() || !r->is_string()) continue;
    auto d = v.find("domain");
    if (d == v.end() || !d->is_string()) continue;

    auto reason = r->get<std::string>();
    auto domain = d->get<std::string>();
    auto metadata = std::unordered_map<std::string, std::string>{};
    auto m = v.find("metadata");
    if (m != v.end() && m->is_object()) {
      for (auto const& i : m->items()) {
        if (!i.value().is_string()) continue;
        metadata[i.key()] = i.value();
      }
    }
    metadata["http_status_code"] = std::to_string(http_status_code);
    // GCS adds some attributes that look useful
    for (auto const* key : {"locationType", "location"}) {
      auto k = v.find(key);
      if (k == v.end() || !k->is_string()) continue;
      metadata[key] = k->get<std::string>();
    }
    return ErrorInfo{std::move(reason), std::move(domain), std::move(metadata)};
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
  auto e = json.find("error");
  if (e == json.end() || !e->is_object()) return err();
  auto m = e->find("message");
  if (m == e->end() || !m->is_string()) return err();
  for (auto const* name : {"details", "errors"}) {
    auto details = e->find(name);
    if (details == e->end()) continue;
    return std::make_pair(m->get<std::string>(),
                          MakeErrorInfo(http_status_code, *details));
  }
  return err();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
