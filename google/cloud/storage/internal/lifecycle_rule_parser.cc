// Copyright 2020 Google LLC
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

#include "google/cloud/storage/internal/lifecycle_rule_parser.h"
#include "google/cloud/storage/internal/common_metadata_parser.h"

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

absl::optional<std::vector<std::string>> ParseStringListCondition(
    nlohmann::json const& condition, char const* name) {
  if (!condition.contains(name)) return absl::nullopt;
  std::vector<std::string> matches;
  for (auto const& kv : condition[name].items()) {
    matches.emplace_back(kv.value().get<std::string>());
  }
  return matches;
}

Status ParseIntCondition(absl::optional<std::int32_t>& field,
                         nlohmann::json const& condition, char const* name) {
  if (!condition.contains(name)) return Status{};
  auto value = internal::ParseIntField(condition, name);
  if (!value) return std::move(value).status();
  field.emplace(*value);
  return Status{};
}

Status ParseBoolCondition(absl::optional<bool>& field,
                          nlohmann::json const& condition, char const* name) {
  if (!condition.contains(name)) return Status{};
  auto value = internal::ParseBoolField(condition, name);
  if (!value) return std::move(value).status();
  field.emplace(*value);
  return Status{};
}

Status ParseDateCondition(absl::optional<absl::CivilDay>& field,
                          nlohmann::json const& condition, char const* name) {
  if (!condition.contains(name)) return Status{};
  auto const date = condition.value(name, "");
  absl::CivilDay day;
  if (!absl::ParseCivilTime(date, &day)) {
    return Status(StatusCode::kInvalidArgument,
                  "Cannot parse " + std::string(name) + " with value=<" + date +
                      "> as a date");
  }
  field.emplace(std::move(day));
  return Status{};
}

}  // namespace

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
  if (json.count("condition") == 0) return result;

  auto condition = json["condition"];

  auto status = ParseIntCondition(result.condition_.age, condition, "age");
  if (!status.ok()) return status;
  status = ParseDateCondition(result.condition_.created_before, condition,
                              "createdBefore");
  if (!status.ok()) return status;
  status = ParseBoolCondition(result.condition_.is_live, condition, "isLive");
  if (!status.ok()) return status;
  result.condition_.matches_storage_class =
      ParseStringListCondition(condition, "matchesStorageClass");
  status = ParseIntCondition(result.condition_.num_newer_versions, condition,
                             "numNewerVersions");
  if (!status.ok()) return status;
  status = ParseIntCondition(result.condition_.days_since_noncurrent_time,
                             condition, "daysSinceNoncurrentTime");
  if (!status.ok()) return status;
  status = ParseDateCondition(result.condition_.noncurrent_time_before,
                              condition, "noncurrentTimeBefore");
  if (!status.ok()) return status;
  status = ParseIntCondition(result.condition_.days_since_custom_time,
                             condition, "daysSinceCustomTime");
  if (!status.ok()) return status;
  status = ParseDateCondition(result.condition_.custom_time_before, condition,
                              "customTimeBefore");
  if (!status.ok()) return status;
  result.condition_.matches_prefix =
      ParseStringListCondition(condition, "matchesPrefix");
  result.condition_.matches_suffix =
      ParseStringListCondition(condition, "matchesSuffix");
  return result;
}

StatusOr<LifecycleRule> LifecycleRuleParser::FromString(
    std::string const& text) {
  auto json = nlohmann::json::parse(text, nullptr, false);
  return FromJson(json);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
