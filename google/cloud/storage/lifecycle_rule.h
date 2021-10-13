// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_LIFECYCLE_RULE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_LIFECYCLE_RULE_H

#include "google/cloud/storage/storage_class.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/internal/parse_rfc3339.h"
#include "google/cloud/optional.h"
#include "google/cloud/status_or.h"
#include "absl/time/civil_time.h"
#include "absl/types/optional.h"
#include <iosfwd>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
struct LifecycleRuleParser;
class GrpcClient;
}  // namespace internal

/// Implement a wrapper for Lifecycle Rules actions.
struct LifecycleRuleAction {
  std::string type;
  std::string storage_class;
};

inline bool operator==(LifecycleRuleAction const& lhs,
                       LifecycleRuleAction const& rhs) {
  return std::tie(lhs.type, lhs.storage_class) ==
         std::tie(rhs.type, rhs.storage_class);
}

inline bool operator<(LifecycleRuleAction const& lhs,
                      LifecycleRuleAction const& rhs) {
  return std::tie(lhs.type, lhs.storage_class) <
         std::tie(rhs.type, rhs.storage_class);
}

inline bool operator!=(LifecycleRuleAction const& lhs,
                       LifecycleRuleAction const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

inline bool operator>(LifecycleRuleAction const& lhs,
                      LifecycleRuleAction const& rhs) {
  return std::rel_ops::operator>(lhs, rhs);
}

inline bool operator<=(LifecycleRuleAction const& lhs,
                       LifecycleRuleAction const& rhs) {
  return std::rel_ops::operator<=(lhs, rhs);
}

inline bool operator>=(LifecycleRuleAction const& lhs,
                       LifecycleRuleAction const& rhs) {
  return std::rel_ops::operator>=(lhs, rhs);
}

std::ostream& operator<<(std::ostream& os, LifecycleRuleAction const& rhs);

/// Implement a wrapper for Lifecycle Conditions.
struct LifecycleRuleCondition {
  absl::optional<std::int32_t> age;
  absl::optional<absl::CivilDay> created_before;
  absl::optional<bool> is_live;
  absl::optional<std::vector<std::string>> matches_storage_class;
  absl::optional<std::int32_t> num_newer_versions;
  absl::optional<std::int32_t> days_since_noncurrent_time;
  absl::optional<absl::CivilDay> noncurrent_time_before;
  absl::optional<std::int32_t> days_since_custom_time;
  absl::optional<absl::CivilDay> custom_time_before;
};

inline bool operator==(LifecycleRuleCondition const& lhs,
                       LifecycleRuleCondition const& rhs) {
  return lhs.age == rhs.age && lhs.created_before == rhs.created_before &&
         lhs.is_live == rhs.is_live &&
         lhs.matches_storage_class == rhs.matches_storage_class &&
         lhs.num_newer_versions == rhs.num_newer_versions &&
         lhs.days_since_noncurrent_time == rhs.days_since_noncurrent_time &&
         lhs.noncurrent_time_before == rhs.noncurrent_time_before &&
         lhs.days_since_custom_time == rhs.days_since_custom_time &&
         lhs.custom_time_before == rhs.custom_time_before;
}

inline bool operator<(LifecycleRuleCondition const& lhs,
                      LifecycleRuleCondition const& rhs) {
  return std::tie(lhs.age, lhs.created_before, lhs.is_live,
                  lhs.matches_storage_class, lhs.num_newer_versions,
                  lhs.days_since_noncurrent_time, lhs.noncurrent_time_before,
                  lhs.days_since_custom_time, lhs.custom_time_before) <
         std::tie(rhs.age, rhs.created_before, rhs.is_live,
                  rhs.matches_storage_class, rhs.num_newer_versions,
                  rhs.days_since_noncurrent_time, rhs.noncurrent_time_before,
                  rhs.days_since_custom_time, rhs.custom_time_before);
}

inline bool operator!=(LifecycleRuleCondition const& lhs,
                       LifecycleRuleCondition const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

inline bool operator>(LifecycleRuleCondition const& lhs,
                      LifecycleRuleCondition const& rhs) {
  return std::rel_ops::operator>(lhs, rhs);
}

inline bool operator<=(LifecycleRuleCondition const& lhs,
                       LifecycleRuleCondition const& rhs) {
  return std::rel_ops::operator<=(lhs, rhs);
}

inline bool operator>=(LifecycleRuleCondition const& lhs,
                       LifecycleRuleCondition const& rhs) {
  return std::rel_ops::operator>=(lhs, rhs);
}

std::ostream& operator<<(std::ostream& os, LifecycleRuleCondition const& rhs);

/**
 * Defines objects to read, create, and modify Object Lifecycle Rules.
 *
 * Object Lifecycle Rules allow to configure a Bucket to automatically delete
 * or change the storage class of objects as they go through lifecycle events.
 *
 * @see https://cloud.google.com/storage/docs/lifecycle for general information
 *     on Object Lifecycle Management in Google Cloud Storage.
 */
class LifecycleRule {
 public:
  explicit LifecycleRule(LifecycleRuleCondition condition,
                         LifecycleRuleAction action)
      : action_(std::move(action)), condition_(std::move(condition)) {}

  LifecycleRuleAction const& action() const { return action_; }
  LifecycleRuleCondition const& condition() const { return condition_; }

  //@{
  /**
   * @name Creates different types of LifecycleRule actions.
   */
  static LifecycleRuleAction Delete();
  static LifecycleRuleAction SetStorageClassStandard();
  static LifecycleRuleAction SetStorageClassMultiRegional();
  static LifecycleRuleAction SetStorageClassRegional();
  static LifecycleRuleAction SetStorageClassNearline();
  static LifecycleRuleAction SetStorageClassColdline();
  static LifecycleRuleAction SetStorageClassDurableReducedAvailability();
  static LifecycleRuleAction SetStorageClassArchive();
  static LifecycleRuleAction SetStorageClass(std::string storage_class);
  //@}

  //@{
  /**
   * @name Creates different types of LifecycleRule rules.
   */
  static LifecycleRuleCondition MaxAge(std::int32_t days) {
    LifecycleRuleCondition result;
    result.age.emplace(std::move(days));
    return result;
  }

  static LifecycleRuleCondition CreatedBefore(absl::CivilDay date) {
    LifecycleRuleCondition result;
    result.created_before.emplace(std::move(date));
    return result;
  }

  static LifecycleRuleCondition IsLive(bool value) {
    LifecycleRuleCondition result;
    result.is_live.emplace(std::move(value));
    return result;
  }

  static LifecycleRuleCondition MatchesStorageClass(std::string storage_class) {
    std::vector<std::string> value;
    value.emplace_back(std::move(storage_class));
    LifecycleRuleCondition result;
    result.matches_storage_class.emplace(std::move(value));
    return result;
  }

  static LifecycleRuleCondition MatchesStorageClasses(
      std::initializer_list<std::string> list) {
    std::vector<std::string> classes(std::move(list));
    LifecycleRuleCondition result;
    result.matches_storage_class.emplace(std::move(classes));
    return result;
  }

  template <typename Iterator>
  static LifecycleRuleCondition MatchesStorageClasses(Iterator begin,
                                                      Iterator end) {
    std::vector<std::string> classes(begin, end);
    LifecycleRuleCondition result;
    result.matches_storage_class.emplace(std::move(classes));
    return result;
  }

  static LifecycleRuleCondition MatchesStorageClassStandard() {
    return MatchesStorageClass(storage_class::Standard());
  }

  static LifecycleRuleCondition MatchesStorageClassMultiRegional() {
    return MatchesStorageClass(storage_class::MultiRegional());
  }

  static LifecycleRuleCondition MatchesStorageClassRegional() {
    return MatchesStorageClass(storage_class::Regional());
  }

  static LifecycleRuleCondition MatchesStorageClassNearline() {
    return MatchesStorageClass(storage_class::Nearline());
  }

  static LifecycleRuleCondition MatchesStorageClassColdline() {
    return MatchesStorageClass(storage_class::Coldline());
  }

  static LifecycleRuleCondition
  MatchesStorageClassDurableReducedAvailability() {
    return MatchesStorageClass(storage_class::DurableReducedAvailability());
  }

  static LifecycleRuleCondition MatchesStorageClassArchive() {
    return MatchesStorageClass(storage_class::Archive());
  }

  static LifecycleRuleCondition NumNewerVersions(std::int32_t days) {
    LifecycleRuleCondition result;
    result.num_newer_versions.emplace(std::move(days));
    return result;
  }

  static LifecycleRuleCondition DaysSinceNoncurrentTime(std::int32_t days) {
    LifecycleRuleCondition result;
    result.days_since_noncurrent_time = days;
    return result;
  }

  static LifecycleRuleCondition NoncurrentTimeBefore(absl::CivilDay date) {
    LifecycleRuleCondition result;
    result.noncurrent_time_before = date;
    return result;
  }

  static LifecycleRuleCondition DaysSinceCustomTime(std::int32_t days) {
    LifecycleRuleCondition result;
    result.days_since_custom_time = days;
    return result;
  }

  static LifecycleRuleCondition CustomTimeBefore(absl::CivilDay date) {
    LifecycleRuleCondition result;
    result.custom_time_before = date;
    return result;
  }
  //@}

  /**
   * Combines multiple LifecycleRule conditions using conjunction.
   *
   * Create a condition that require all the @p condition parameters to be met
   * to take effect.
   *
   * @par Example
   *
   * @code
   * // Affect objects that are in the STANDARD storage class, have at
   * // least 2 new versions, are at least 7 days old, and are alive.
   * LifecycleRuleCondition condition = LifecycleRule::ConditionConjunction(
   *     LifecycleRule::NumNewerVersions(2),
   *     LifecycleRule::MatchesStorageClassStandard(),
   *     LifecycleRule::MaxAge(7), LifecycleRule::IsLive(true));
   * @endcode
   *
   * @throws std::invalid_argument if the list of parameters is contradictory,
   *     for example, `IsLive(true)` and `IsLive(false)` are in the @p condition
   *     list.
   * @return a LifecycleRuleCondition that is satisfied when all the
   *     @p condition conditions are satisfied.
   * @tparam Condition the types of the conditions, they must all be convertible
   *     to `LifecycleRuleCondition`.
   */
  template <typename... Condition>
  static LifecycleRuleCondition ConditionConjunction(Condition&&... condition) {
    LifecycleRuleCondition result;
    MergeConditions(result, std::forward<Condition>(condition)...);
    return result;
  }

 private:
  friend struct internal::LifecycleRuleParser;
  friend class internal::GrpcClient;

  LifecycleRule() = default;

  static void MergeConditions(LifecycleRuleCondition& result,
                              LifecycleRuleCondition const& rhs);

  template <typename... Condition>
  static void MergeConditions(LifecycleRuleCondition& result,
                              LifecycleRuleCondition const& head,
                              Condition&&... tail) {
    MergeConditions(result, head);
    MergeConditions(result, std::forward<Condition>(tail)...);
  }

  LifecycleRuleAction action_;
  LifecycleRuleCondition condition_;
};

inline bool operator==(LifecycleRule const& lhs, LifecycleRule const& rhs) {
  return std::tie(lhs.condition(), lhs.action()) ==
         std::tie(rhs.condition(), rhs.action());
}

inline bool operator<(LifecycleRule const& lhs, LifecycleRule const& rhs) {
  return std::tie(lhs.action(), lhs.condition()) <
         std::tie(rhs.action(), rhs.condition());
}

inline bool operator!=(LifecycleRule const& lhs, LifecycleRule const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

inline bool operator>(LifecycleRule const& lhs, LifecycleRule const& rhs) {
  return std::rel_ops::operator>(lhs, rhs);
}

inline bool operator<=(LifecycleRule const& lhs, LifecycleRule const& rhs) {
  return std::rel_ops::operator<=(lhs, rhs);
}

inline bool operator>=(LifecycleRule const& lhs, LifecycleRule const& rhs) {
  return std::rel_ops::operator>=(lhs, rhs);
}

std::ostream& operator<<(std::ostream& os, LifecycleRule const& rhs);
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_LIFECYCLE_RULE_H
