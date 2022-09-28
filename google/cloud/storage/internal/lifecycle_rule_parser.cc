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
#include "google/cloud/storage/internal/metadata_parser.h"

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

StatusOr<LifecycleRuleAction> ActionFromJson(nlohmann::json const& json) {
  auto f = json.find("action");
  if (f == json.end()) return LifecycleRuleAction{};
  if (!f->is_object()) return Status(StatusCode::kInvalidArgument, __func__);

  LifecycleRuleAction action;
  action.type = f->value("type", "");
  action.storage_class = f->value("storageClass", "");
  return action;
}

StatusOr<LifecycleRuleCondition> ConditionFromJson(nlohmann::json const& json) {
  auto f = json.find("condition");
  if (f == json.end()) return LifecycleRuleCondition{};
  if (!f->is_object()) return Status(StatusCode::kInvalidArgument, __func__);

  LifecycleRuleCondition result;
  auto status = ParseIntCondition(result.age, *f, "age");
  if (!status.ok()) return status;
  status = ParseDateCondition(result.created_before, *f, "createdBefore");
  if (!status.ok()) return status;
  status = ParseBoolCondition(result.is_live, *f, "isLive");
  if (!status.ok()) return status;
  result.matches_storage_class =
      ParseStringListCondition(*f, "matchesStorageClass");
  status = ParseIntCondition(result.num_newer_versions, *f, "numNewerVersions");
  if (!status.ok()) return status;
  status = ParseIntCondition(result.days_since_noncurrent_time, *f,
                             "daysSinceNoncurrentTime");
  if (!status.ok()) return status;
  status = ParseDateCondition(result.noncurrent_time_before, *f,
                              "noncurrentTimeBefore");
  if (!status.ok()) return status;
  status = ParseIntCondition(result.days_since_custom_time, *f,
                             "daysSinceCustomTime");
  if (!status.ok()) return status;
  status =
      ParseDateCondition(result.custom_time_before, *f, "customTimeBefore");
  if (!status.ok()) return status;
  result.matches_prefix = ParseStringListCondition(*f, "matchesPrefix");
  result.matches_suffix = ParseStringListCondition(*f, "matchesSuffix");
  return result;
}

}  // namespace

StatusOr<LifecycleRule> LifecycleRuleParser::FromJson(
    nlohmann::json const& json) {
  if (!json.is_object()) return Status(StatusCode::kInvalidArgument, __func__);
  auto condition = ConditionFromJson(json);
  if (!condition) return std::move(condition).status();
  auto action = ActionFromJson(json);
  if (!action) return std::move(action).status();

  return LifecycleRule(*std::move(condition), *std::move(action));
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
