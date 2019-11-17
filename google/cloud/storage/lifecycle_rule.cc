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

#include "google/cloud/storage/lifecycle_rule.h"
#include "google/cloud/storage/internal/metadata_parser.h"
#include <algorithm>
#include <iostream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
std::ostream& operator<<(std::ostream& os, LifecycleRuleAction const& rhs) {
  return os << "LifecycleRuleAction={" << rhs.type << ", " << rhs.storage_class
            << "}";
}

LifecycleRuleAction LifecycleRule::Delete() {
  return LifecycleRuleAction{"Delete", {}};
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
    os << sep
       << "created_before=" << rhs.created_before->time_since_epoch().count();
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
    sep = "";
    for (auto const& s : *rhs.matches_storage_class) {
      os << sep << s;
      sep = ", ";
    }
    os << "]";
    sep = ", ";
  }
  if (rhs.num_newer_versions.has_value()) {
    os << sep << "num_newer_versions=" << *rhs.num_newer_versions;
  }
  return os << "}";
}

void LifecycleRule::MergeConditions(LifecycleRuleCondition& result,
                                    LifecycleRuleCondition const& rhs) {
  if (rhs.age.has_value()) {
    if (result.age.has_value()) {
      *result.age = std::min(*result.age, *rhs.age);
    } else {
      auto tmp = *rhs.age;
      result.age.emplace(std::forward<std::int32_t>(tmp));
    }
  }
  if (rhs.created_before.has_value()) {
    if (result.created_before.has_value()) {
      *result.created_before =
          std::max(*result.created_before, *rhs.created_before);
    } else {
      auto tmp = *rhs.created_before;
      result.created_before.emplace(
          std::forward<std::chrono::system_clock::time_point>(tmp));
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
  if (rhs.matches_storage_class.has_value()) {
    if (result.matches_storage_class.has_value()) {
      std::vector<std::string> tmp;
      std::vector<std::string> a = *std::move(result.matches_storage_class);
      std::sort(a.begin(), a.end());
      std::vector<std::string> b = *rhs.matches_storage_class;
      std::sort(b.begin(), b.end());
      std::set_intersection(a.begin(), a.end(), b.begin(), b.end(),
                            std::back_inserter(tmp));
      result.matches_storage_class.emplace(std::move(tmp));
    } else {
      auto tmp = *rhs.matches_storage_class;
      result.matches_storage_class.emplace(std::move(tmp));
    }
  }
  if (rhs.num_newer_versions.has_value()) {
    if (result.num_newer_versions.has_value()) {
      *result.num_newer_versions =
          std::max(*result.num_newer_versions, *rhs.num_newer_versions);
    } else {
      auto tmp = *rhs.num_newer_versions;
      result.num_newer_versions.emplace(std::forward<std::int32_t>(tmp));
    }
  }
}

std::ostream& operator<<(std::ostream& os, LifecycleRule const& rhs) {
  return os << "LifecycleRule={condition=" << rhs.condition()
            << ", action=" << rhs.action() << "}";
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
