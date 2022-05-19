// Copyright 2018 Google LLC
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

#include "google/cloud/storage/lifecycle_rule.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include <algorithm>
#include <iostream>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

absl::optional<std::vector<std::string>> MergeStringListConditions(
    absl::optional<std::vector<std::string>> result,
    absl::optional<std::vector<std::string>> const& rhs) {
  if (!rhs.has_value()) return result;
  if (!result.has_value()) return *rhs;

  std::sort(result->begin(), result->end());
  std::vector<std::string> b = *rhs;
  std::sort(b.begin(), b.end());
  std::vector<std::string> tmp;
  tmp.reserve((std::max)(result->size(), b.size()));
  std::set_intersection(result->begin(), result->end(), b.begin(), b.end(),
                        std::back_inserter(tmp));
  tmp.shrink_to_fit();
  return tmp;
}

}  // namespace

bool operator==(LifecycleRuleCondition const& lhs,
                LifecycleRuleCondition const& rhs) {
  return lhs.age == rhs.age && lhs.created_before == rhs.created_before &&
         lhs.is_live == rhs.is_live &&
         lhs.matches_storage_class == rhs.matches_storage_class &&
         lhs.num_newer_versions == rhs.num_newer_versions &&
         lhs.days_since_noncurrent_time == rhs.days_since_noncurrent_time &&
         lhs.noncurrent_time_before == rhs.noncurrent_time_before &&
         lhs.days_since_custom_time == rhs.days_since_custom_time &&
         lhs.custom_time_before == rhs.custom_time_before &&
         lhs.matches_prefix == rhs.matches_prefix &&
         lhs.matches_suffix == rhs.matches_suffix;
}

bool operator<(LifecycleRuleCondition const& lhs,
               LifecycleRuleCondition const& rhs) {
  return std::tie(lhs.age, lhs.created_before, lhs.is_live,
                  lhs.matches_storage_class, lhs.num_newer_versions,
                  lhs.days_since_noncurrent_time, lhs.noncurrent_time_before,
                  lhs.days_since_custom_time, lhs.custom_time_before,
                  lhs.matches_prefix, lhs.matches_suffix) <
         std::tie(rhs.age, rhs.created_before, rhs.is_live,
                  rhs.matches_storage_class, rhs.num_newer_versions,
                  rhs.days_since_noncurrent_time, rhs.noncurrent_time_before,
                  rhs.days_since_custom_time, rhs.custom_time_before,
                  rhs.matches_prefix, rhs.matches_suffix);
}

std::ostream& operator<<(std::ostream& os, LifecycleRuleAction const& rhs) {
  return os << "LifecycleRuleAction={" << rhs.type << ", " << rhs.storage_class
            << "}";
}

LifecycleRuleAction LifecycleRule::Delete() {
  return LifecycleRuleAction{"Delete", {}};
}

LifecycleRuleAction LifecycleRule::AbortIncompleteMultipartUpload() {
  return LifecycleRuleAction{"AbortIncompleteMultipartUpload", {}};
}

LifecycleRuleAction LifecycleRule::SetStorageClassStandard() {
  return SetStorageClass(storage_class::Standard());
}

LifecycleRuleAction LifecycleRule::SetStorageClassMultiRegional() {
  return SetStorageClass(storage_class::MultiRegional());
}

LifecycleRuleAction LifecycleRule::SetStorageClassRegional() {
  return SetStorageClass(storage_class::Regional());
}

LifecycleRuleAction LifecycleRule::SetStorageClassNearline() {
  return SetStorageClass(storage_class::Nearline());
}

LifecycleRuleAction LifecycleRule::SetStorageClassColdline() {
  return SetStorageClass(storage_class::Coldline());
}

LifecycleRuleAction LifecycleRule::SetStorageClassDurableReducedAvailability() {
  return SetStorageClass(storage_class::DurableReducedAvailability());
}

LifecycleRuleAction LifecycleRule::SetStorageClassArchive() {
  return SetStorageClass(storage_class::Archive());
}

LifecycleRuleAction LifecycleRule::SetStorageClass(std::string storage_class) {
  return LifecycleRuleAction{"SetStorageClass", std::move(storage_class)};
}

std::ostream& operator<<(std::ostream& os, LifecycleRuleCondition const& rhs) {
  os << "LifecycleRuleCondition={";
  char const* sep = "";
  if (rhs.age.has_value()) {
    os << sep << "age=" << *rhs.age;
    sep = ", ";
  }
  if (rhs.created_before.has_value()) {
    os << sep << "created_before=" << *rhs.created_before;
    sep = ", ";
  }
  if (rhs.is_live.has_value()) {
    auto flags = os.flags();
    os << sep << "is_live=" << std::boolalpha << *rhs.is_live;
    os.flags(flags);
    sep = ", ";
  }
  if (rhs.matches_storage_class.has_value()) {
    os << sep << "matches_storage_class=[";
    os << absl::StrJoin(*rhs.matches_storage_class, ", ");
    os << "]";
    sep = ", ";
  }
  if (rhs.num_newer_versions.has_value()) {
    os << sep << "num_newer_versions=" << *rhs.num_newer_versions;
    sep = ", ";
  }
  if (rhs.days_since_noncurrent_time.has_value()) {
    os << sep
       << "days_since_noncurrent_time=" << *rhs.days_since_noncurrent_time;
    sep = ", ";
  }
  if (rhs.noncurrent_time_before.has_value()) {
    os << sep << "noncurrent_time_before=" << *rhs.noncurrent_time_before;
    sep = ", ";
  }
  if (rhs.days_since_custom_time.has_value()) {
    os << sep << "days_since_custom_time=" << *rhs.days_since_custom_time;
    sep = ", ";
  }
  if (rhs.custom_time_before.has_value()) {
    os << sep << "custom_time_before=" << *rhs.custom_time_before;
  }
  if (rhs.matches_prefix.has_value()) {
    os << sep << "matches_prefix=[";
    os << absl::StrJoin(*rhs.matches_prefix, ", ");
    os << "]";
    sep = ", ";
  }
  if (rhs.matches_suffix.has_value()) {
    os << sep << "matches_suffix=[";
    os << absl::StrJoin(*rhs.matches_suffix, ", ");
    os << "]";
    sep = ", ";
  }
  return os << "}";
}

void LifecycleRule::MergeConditions(LifecycleRuleCondition& result,
                                    LifecycleRuleCondition const& rhs) {
  if (rhs.age.has_value()) {
    if (result.age.has_value()) {
      result.age = std::min(*result.age, *rhs.age);
    } else {
      auto tmp = *rhs.age;
      result.age.emplace(std::forward<std::int32_t>(tmp));
    }
  }
  if (rhs.created_before.has_value()) {
    if (result.created_before.has_value()) {
      result.created_before =
          std::max(*result.created_before, *rhs.created_before);
    } else {
      result.created_before.emplace(std::move(*rhs.created_before));
    }
  }
  if (rhs.is_live.has_value()) {
    if (result.is_live.has_value()) {
      if (result.is_live.value() != rhs.is_live.value()) {
        google::cloud::internal::ThrowInvalidArgument(
            "Cannot set is_live to both true and false in LifecycleRule "
            "condition");
      }
    } else {
      auto tmp = *rhs.is_live;
      result.is_live.emplace(std::forward<bool>(tmp));
    }
  }
  result.matches_storage_class = MergeStringListConditions(
      std::move(result.matches_storage_class), rhs.matches_storage_class);
  if (rhs.num_newer_versions.has_value()) {
    if (result.num_newer_versions.has_value()) {
      result.num_newer_versions =
          std::max(*result.num_newer_versions, *rhs.num_newer_versions);
    } else {
      auto tmp = *rhs.num_newer_versions;
      result.num_newer_versions.emplace(std::forward<std::int32_t>(tmp));
    }
  }
  if (rhs.days_since_noncurrent_time.has_value()) {
    if (result.days_since_noncurrent_time.has_value()) {
      result.days_since_noncurrent_time = (std::max)(
          *result.days_since_noncurrent_time, *rhs.days_since_noncurrent_time);
    } else {
      result.days_since_noncurrent_time = *rhs.days_since_noncurrent_time;
    }
  }
  if (rhs.noncurrent_time_before.has_value()) {
    if (result.noncurrent_time_before.has_value()) {
      result.noncurrent_time_before = (std::min)(*result.noncurrent_time_before,
                                                 *rhs.noncurrent_time_before);
    } else {
      result.noncurrent_time_before = *rhs.noncurrent_time_before;
    }
  }
  if (rhs.days_since_custom_time.has_value()) {
    if (result.days_since_custom_time.has_value()) {
      result.days_since_custom_time = (std::max)(*result.days_since_custom_time,
                                                 *rhs.days_since_custom_time);
    } else {
      result.days_since_custom_time = *rhs.days_since_custom_time;
    }
  }
  if (rhs.custom_time_before.has_value()) {
    if (result.custom_time_before.has_value()) {
      result.custom_time_before =
          (std::min)(*result.custom_time_before, *rhs.custom_time_before);
    } else {
      result.custom_time_before = *rhs.custom_time_before;
    }
  }
  result.matches_prefix = MergeStringListConditions(
      std::move(result.matches_prefix), rhs.matches_prefix);
  result.matches_suffix = MergeStringListConditions(
      std::move(result.matches_suffix), rhs.matches_suffix);
}

std::ostream& operator<<(std::ostream& os, LifecycleRule const& rhs) {
  return os << "LifecycleRule={condition=" << rhs.condition()
            << ", action=" << rhs.action() << "}";
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
