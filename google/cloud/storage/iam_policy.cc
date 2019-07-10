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

#include "google/cloud/storage/iam_policy.h"
#include "google/cloud/storage/version.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
internal::nl::json Expression(std::string expression, std::string title,
                              std::string description, std::string location) {
  internal::nl::json res{{"expression", std::move(expression)}};
  if (!title.empty()) {
    res["title"] = std::move(title);
  }
  if (!description.empty()) {
    res["description"] = std::move(description);
  }
  if (!location.empty()) {
    res["location"] = std::move(location);
  }
  return res;
}

internal::nl::json IamBinding(std::string role,
                              std::initializer_list<std::string> members) {
  return IamBinding(std::move(role), members.begin(), members.end());
}

internal::nl::json IamBinding(std::string role,
                              std::initializer_list<std::string> members,
                              internal::nl::json condition) {
  return IamBindingSetCondition(IamBinding(std::move(role), members),
                                std::move(condition));
}

internal::nl::json IamBinding(std::string role,
                              std::vector<std::string> members) {
  internal::nl::json res{{"role", std::move(role)}};
  for (auto& member : members) {
    res["members"].emplace_back(std::move(member));
  }
  return res;
}

internal::nl::json IamBinding(std::string role,
                              std::vector<std::string> members,
                              internal::nl::json condition) {
  return IamBindingSetCondition(IamBinding(std::move(role), std::move(members)),
                                std::move(condition));
}

internal::nl::json IamBindingSetCondition(internal::nl::json binding,
                                          internal::nl::json condition) {
  binding["condition"] = std::move(condition);
  return binding;
}

internal::nl::json IamPolicy(std::initializer_list<internal::nl::json> bindings,
                             std::string etag, std::int32_t version) {
  return IamPolicy(bindings.begin(), bindings.end(), std::move(etag), version);
}

internal::nl::json IamPolicy(std::vector<internal::nl::json> bindings,
                             std::string etag, std::int32_t version) {
  internal::nl::json res{{"kind", "storage#policy"}, {"version", version}};
  if (!etag.empty()) {
    res["etag"] = etag;
  }
  for (auto& binding : bindings) {
    res["bindings"].emplace_back(binding);
  }
  return res;
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
