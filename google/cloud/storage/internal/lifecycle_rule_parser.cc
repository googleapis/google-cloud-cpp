// Copyright 2020 Google LLC
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

#include "google/cloud/storage/internal/lifecycle_rule_parser.h"
#include "google/cloud/storage/internal/common_metadata_parser.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
StatusOr<LifecycleRule> LifecycleRuleParser::FromJson(
    nlohmann::json const& json) {
  if (!json.is_object()) {
    return Status(StatusCode::kInvalidArgument, __func__);
  }
  LifecycleRule result;
  if (json.count("action") != 0) {
    result.action_.type = json["action"].value("type", "");
    result.action_.storage_class = json["action"].value("storageClass", "");
  }
  if (json.count("condition") != 0) {
    auto condition = json["condition"];
    if (condition.count("age") != 0) {
      auto age = internal::ParseIntField(condition, "age");
      if (!age) return std::move(age).status();
      result.condition_.age.emplace(*age);
    }
    if (condition.count("createdBefore") != 0) {
      auto const date = condition.value("createdBefore", "");
      absl::CivilDay day;
      if (!absl::ParseCivilTime(date, &day)) {
        return Status(
            StatusCode::kInvalidArgument,
            "Cannot parse createdBefore value (" + date + ") as a date");
      }
      result.condition_.created_before.emplace(std::move(day));
    }
    if (condition.count("isLive") != 0) {
      auto is_live = internal::ParseBoolField(condition, "isLive");
      if (!is_live.ok()) return std::move(is_live).status();
      result.condition_.is_live.emplace(*is_live);
    }
    if (condition.count("matchesStorageClass") != 0) {
      std::vector<std::string> matches;
      for (auto const& kv : condition["matchesStorageClass"].items()) {
        matches.emplace_back(kv.value().get<std::string>());
      }
      result.condition_.matches_storage_class.emplace(std::move(matches));
    }
    if (condition.count("numNewerVersions") != 0) {
      auto v = internal::ParseIntField(condition, "numNewerVersions");
      if (!v) return std::move(v).status();
      result.condition_.num_newer_versions.emplace(*v);
    }
    if (condition.count("daysSinceNoncurrentTime") != 0) {
      auto v = internal::ParseIntField(condition, "daysSinceNoncurrentTime");
      if (!v) return std::move(v).status();
      result.condition_.days_since_noncurrent_time.emplace(*v);
    }
    if (condition.count("noncurrentTimeBefore") != 0) {
      auto const date = condition.value("noncurrentTimeBefore", "");
      absl::CivilDay day;
      if (!absl::ParseCivilTime(date, &day)) {
        return Status(
            StatusCode::kInvalidArgument,
            "Cannot parse noncurrentTimeBefore value (" + date + ") as a date");
      }
      result.condition_.noncurrent_time_before.emplace(std::move(day));
    }
    if (condition.count("daysSinceCustomTime") != 0) {
      auto v = internal::ParseIntField(condition, "daysSinceCustomTime");
      if (!v) return std::move(v).status();
      result.condition_.days_since_custom_time.emplace(*v);
    }
    if (condition.count("customTimeBefore") != 0) {
      auto const date = condition.value("customTimeBefore", "");
      absl::CivilDay day;
      if (!absl::ParseCivilTime(date, &day)) {
        return Status(
            StatusCode::kInvalidArgument,
            "Cannot parse customTimeBefore value (" + date + ") as a date");
      }
      result.condition_.custom_time_before.emplace(std::move(day));
    }
  }
  return result;
}

StatusOr<LifecycleRule> LifecycleRuleParser::FromString(
    std::string const& text) {
  auto json = nlohmann::json::parse(text, nullptr, false);
  return FromJson(json);
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
