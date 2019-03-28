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

#include "google/cloud/storage/internal/policy_document_requests.h"
#include "google/cloud/storage/internal/format_time_point.h"
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

std::string PolicyDocumentRequest::StringToSign() const {
  using internal::nl::json;
  auto document = policy_document();

  json j;
  j["expiration"] = internal::FormatRfc3339(document.expiration_time);

  for (auto const& kv : document.conditions) {
    PolicyDocumentEntry condition = kv.entry();
    auto elements = kv.entry().elements;

    if (elements.size() == 2) {
      json object;
      object[elements.at(0)] = elements.at(1);
      j["conditions"].push_back(object);
    } else {
      j["conditions"].push_back(elements);
    }
  }

  return std::move(j).dump();
}

StatusOr<PolicyDocument> PolicyDocumentParser::FromJson(
    internal::nl::json const& json) {
  if (!json.is_object()) {
    return Status(StatusCode::kInvalidArgument, __func__);
  }
  PolicyDocument result;

  if (json.count("expiration") != 0) {
    result.expiration_time =
        internal::ParseRfc3339(json["expiration"].get<std::string>());
  }

  /* This ugly code needs to account for entries like:
   *
   *  {"acl": "bucket-owner-read" }
   *  ["eq", "$Content-Type", "image/jpeg" ]
   *  ["content-length-range", 0, 1000000]
   *
   * Notice how the entry can be a string or an array and that the values can
   * either contain strings or numbers. To deal with objects, we can get the
   * value as an object_t, which returns a map. Since "there is no way for a
   * JSON value to "know" whether it is stored in an object" (see
   * https://github.com/nlohmann/json/issues/67), we need to iterate over the
   * map; or just use begin() since there is only one entry.
   *
   * For the arrays, we need to iterate over it and convert any potential
   * numbers to strings.
   */
  if (json.count("conditions") != 0) {
    for (auto const& kv : json["conditions"].items()) {
      PolicyDocumentCondition condition;
      if (kv.value().is_object()) {
        auto object = kv.value().get<nl::json::object_t>();
        auto object_key = object.begin()->first;
        auto object_value = object.begin()->second;
        condition.entry_.elements.emplace_back(object_key);
        if (object_value.is_number()) {
          condition.entry_.elements.emplace_back(
              std::to_string(object_value.get<std::int32_t>()));
        } else {
          condition.entry_.elements.emplace_back(
              object_value.get<std::string>());
        }
      } else if (kv.value().is_array()) {
        for (auto const& e : kv.value()) {
          if (e.is_number()) {
            condition.entry_.elements.emplace_back(
                std::to_string(e.get<std::int32_t>()));
          } else {
            condition.entry_.elements.emplace_back(e.get<std::string>());
          }
        }
      }
      result.conditions.emplace_back(std::move(condition));
    }
  }
  return result;
}

StatusOr<PolicyDocument> PolicyDocumentParser::FromString(
    std::string const& text) {
  auto json = internal::nl::json::parse(text, nullptr, false);
  return FromJson(json);
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
