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
#include "absl/types/optional.h"
#include <nlohmann/json.hpp>
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
template <typename Functor>
Status IsOfTypeIfPresent(nlohmann::json const& json,
                         std::string const& json_rep, std::string const& field,
                         std::string const& location_desc, Functor const& check,
                         std::string const& type_desc) {
  if (!field.empty() && json.find(field) == json.end()) {
    return Status();
  }
  nlohmann::json const& to_check = field.empty() ? json : json[field];
  if (!check(to_check)) {
    std::ostringstream os;
    os << "Invalid IamPolicy payload, expected " << type_desc << " for "
       << location_desc << ". payload=" << json_rep;
    return Status(StatusCode::kInvalidArgument, os.str());
  }
  return Status();
}

Status IsStringIfPresent(nlohmann::json const& json,
                         std::string const& json_rep, std::string const& field,
                         std::string const& location_desc) {
  return IsOfTypeIfPresent(
      json, json_rep, field, location_desc,
      [](nlohmann::json const& json) { return json.is_string(); }, "string");
}

Status IsIntIfPresent(nlohmann::json const& json, std::string const& json_rep,
                      std::string const& field,
                      std::string const& location_desc) {
  return IsOfTypeIfPresent(
      json, json_rep, field, location_desc,
      [](nlohmann::json const& json) { return json.is_number_integer(); },
      "integer");
}

Status IsObjectIfPresent(nlohmann::json const& json,
                         std::string const& json_rep, std::string const& field,
                         std::string const& location_desc) {
  return IsOfTypeIfPresent(
      json, json_rep, field, location_desc,
      [](nlohmann::json const& json) { return json.is_object(); }, "object");
}

Status IsArrayIfPresent(nlohmann::json const& json, std::string const& json_rep,
                        std::string const& field,
                        std::string const& location_desc) {
  return IsOfTypeIfPresent(
      json, json_rep, field, location_desc,
      [](nlohmann::json const& json) { return json.is_array(); }, "array");
}

}  // namespace

struct NativeExpression::Impl {
  static StatusOr<NativeExpression> CreateFromJson(
      nlohmann::json const& json, std::string const& policy_json_rep) {
    Status status = IsStringIfPresent(json, policy_json_rep, "expression",
                                      "'expression' field");
    if (!status.ok()) {
      return status;
    }
    status = IsStringIfPresent(json, policy_json_rep, "title", "'title' field");
    if (!status.ok()) {
      return status;
    }
    status = IsStringIfPresent(json, policy_json_rep, "description",
                               "'description' field");
    if (!status.ok()) {
      return status;
    }
    status = IsStringIfPresent(json, policy_json_rep, "location",
                               "'location' field");
    if (!status.ok()) {
      return status;
    }
    // Cannot use make_unique because we are using brace-initialization.
    // NOLINTNEXTLINE(modernize-make-unique)
    return NativeExpression(std::unique_ptr<Impl>(new Impl{json}));
  }

  nlohmann::json ToJson() const { return native_json; }

  nlohmann::json native_json;
};

NativeExpression::NativeExpression(std::string expression, std::string title,
                                   std::string description,
                                   std::string location)
    : pimpl_(new Impl{nlohmann::json{{"expression", std::move(expression)}}}) {
  if (!title.empty()) {
    pimpl_->native_json["title"] = std::move(title);
  }
  if (!description.empty()) {
    pimpl_->native_json["description"] = std::move(description);
  }
  if (!location.empty()) {
    pimpl_->native_json["location"] = std::move(location);
  }
}

NativeExpression::NativeExpression(NativeExpression const& other)
    : pimpl_(new Impl(*other.pimpl_)) {}

NativeExpression::~NativeExpression() = default;

NativeExpression::NativeExpression(std::unique_ptr<Impl> impl)
    : pimpl_(std::move(impl)) {}

NativeExpression& NativeExpression::operator=(NativeExpression const& other) {
  *pimpl_ = *other.pimpl_;
  return *this;
}

NativeExpression::NativeExpression(NativeExpression&& rhs) noexcept
    : pimpl_(std::move(rhs.pimpl_)) {}

NativeExpression& NativeExpression::operator=(NativeExpression&& rhs) noexcept {
  pimpl_ = std::move(rhs.pimpl_);
  return *this;
}

std::string NativeExpression::expression() const {
  return pimpl_->native_json.value("expression", "");
}

void NativeExpression::set_expression(std::string expression) {
  pimpl_->native_json["expression"] = std::move(expression);
}

std::string NativeExpression::title() const {
  return pimpl_->native_json.value("title", "");
}

void NativeExpression::set_title(std::string title) {
  pimpl_->native_json["title"] = std::move(title);
}

std::string NativeExpression::description() const {
  return pimpl_->native_json.value("description", "");
}

void NativeExpression::set_description(std::string description) {
  pimpl_->native_json["description"] = std::move(description);
}

std::string NativeExpression::location() const {
  return pimpl_->native_json.value("location", "");
}

void NativeExpression::set_location(std::string location) {
  pimpl_->native_json["location"] = std::move(location);
}

std::ostream& operator<<(std::ostream& stream, NativeExpression const& e) {
  stream << "(" << e.expression();
  if (!e.title().empty()) {
    stream << ", title=\"" << e.title() << "\"";
  }
  if (!e.description().empty()) {
    stream << ", description=\"" << e.description() << "\"";
  }
  if (!e.location().empty()) {
    stream << ", location=\"" << e.location() << "\"";
  }
  stream << ")";
  return stream;
}

struct NativeIamBinding::Impl {
  static StatusOr<NativeIamBinding> CreateFromJson(
      nlohmann::json json, std::string const& policy_json_rep) {
    Status status =
        IsObjectIfPresent(json, policy_json_rep, "", "'bindings' entry");
    if (!status.ok()) {
      return status;
    }
    status = IsStringIfPresent(json, policy_json_rep, "role", "'role' field");
    if (!status.ok()) {
      return status;
    }
    std::string role = json.value("role", "");
    status =
        IsArrayIfPresent(json, policy_json_rep, "members", "'members' field");
    if (!status.ok()) {
      return status;
    }
    std::vector<std::string> members;
    auto members_it = json.find("members");
    if (members_it != json.end()) {
      for (auto const& member : *members_it) {
        status =
            IsStringIfPresent(member, policy_json_rep, "", "'members' entry");
        if (!status.ok()) {
          return status;
        }
        members.emplace_back(member.get<std::string>());
      }
      json.erase(members_it);
    }
    status = IsObjectIfPresent(json, policy_json_rep, "condition",
                               "'condition' field");
    if (!status.ok()) {
      return status;
    }
    absl::optional<NativeExpression> condition;
    auto condition_it = json.find("condition");
    if (condition_it != json.end()) {
      auto parsed_condition = NativeExpression::Impl::CreateFromJson(
          *condition_it, policy_json_rep);
      if (!parsed_condition) {
        return parsed_condition.status();
      }
      condition = *std::move(parsed_condition);
      json.erase(condition_it);
    }
    // Cannot use make_unique because we are using brace-initialization.
    // NOLINTNEXTLINE(modernize-make-unique)
    return NativeIamBinding(std::unique_ptr<Impl>(
        new Impl{json, std::move(members), std::move(condition)}));
  }

  nlohmann::json ToJson() const {
    auto ret = native_json;
    if (condition.has_value()) {
      ret["condition"] = condition->pimpl_->ToJson();
    }
    if (!members.empty()) {
      ret["members"] = members;
    }
    return ret;
  }

  nlohmann::json native_json;
  std::vector<std::string> members;
  absl::optional<NativeExpression> condition;
};

NativeIamBinding::NativeIamBinding(std::string role,
                                   std::vector<std::string> members)
    : pimpl_(new Impl{nlohmann::json{{"role", std::move(role)}},
                      std::move(members), absl::optional<NativeExpression>()}) {
}

NativeIamBinding::NativeIamBinding(std::string role,
                                   std::vector<std::string> members,
                                   NativeExpression condition)
    : pimpl_(new Impl{nlohmann::json{{"role", std::move(role)}},
                      std::move(members), std::move(condition)}) {}

NativeIamBinding::NativeIamBinding(NativeIamBinding const& other)
    : pimpl_(new Impl(*other.pimpl_)) {}

NativeIamBinding::NativeIamBinding(std::unique_ptr<Impl> impl)
    : pimpl_(std::move(impl)) {}

NativeIamBinding::~NativeIamBinding() = default;

NativeIamBinding& NativeIamBinding::operator=(NativeIamBinding const& other) {
  *pimpl_ = *other.pimpl_;
  return *this;
}

NativeIamBinding::NativeIamBinding(NativeIamBinding&& rhs) noexcept
    : pimpl_(std::move(rhs.pimpl_)) {}

NativeIamBinding& NativeIamBinding::operator=(NativeIamBinding&& rhs) noexcept {
  pimpl_ = std::move(rhs.pimpl_);
  return *this;
}

std::string NativeIamBinding::role() const {
  return pimpl_->native_json.value("role", "");
}

void NativeIamBinding::set_role(std::string role) {
  pimpl_->native_json["role"] = std::move(role);
}

std::vector<std::string> const& NativeIamBinding::members() const {
  return pimpl_->members;
}

std::vector<std::string>& NativeIamBinding::members() {
  return pimpl_->members;
}

NativeExpression const& NativeIamBinding::condition() const {
  return *pimpl_->condition;
}

NativeExpression& NativeIamBinding::condition() { return *pimpl_->condition; }

void NativeIamBinding::set_condition(NativeExpression condition) {
  pimpl_->condition = std::move(condition);
}

bool NativeIamBinding::has_condition() const {
  return pimpl_->condition.has_value();
}

void NativeIamBinding::clear_condition() { pimpl_->condition.reset(); }

std::ostream& operator<<(std::ostream& os, NativeIamBinding const& binding) {
  os << binding.role() << ": [";
  bool first = true;
  for (auto const& member : binding.members()) {
    os << (first ? "" : ", ") << member;
    first = false;
  }
  os << "]";
  if (binding.has_condition()) {
    os << " when " << binding.condition();
  }
  return os;
}

struct NativeIamPolicy::Impl {
  nlohmann::json ToJson() const {
    auto ret = native_json;
    if (!bindings.empty()) {
      auto& ret_bindings = ret["bindings"] = nlohmann::json::array();
      std::transform(bindings.begin(), bindings.end(),
                     std::back_inserter(ret_bindings),
                     [](NativeIamBinding const& binding) {
                       return binding.pimpl_->ToJson();
                     });
    }
    ret["kind"] = "storage#policy";
    return ret;
  }

  nlohmann::json native_json;
  std::vector<NativeIamBinding> bindings;
};

NativeIamPolicy::NativeIamPolicy(std::vector<NativeIamBinding> bindings,
                                 std::string etag, std::int32_t version)
    : pimpl_(
          new Impl{nlohmann::json{{"version", version}}, std::move(bindings)}) {
  if (!etag.empty()) {
    pimpl_->native_json["etag"] = std::move(etag);
  }
}

NativeIamPolicy::NativeIamPolicy(NativeIamPolicy const& other)
    : pimpl_(new Impl(*other.pimpl_)) {}

NativeIamPolicy::NativeIamPolicy(std::unique_ptr<Impl> impl)
    : pimpl_(std::move(impl)) {}

NativeIamPolicy::~NativeIamPolicy() = default;

StatusOr<NativeIamPolicy> NativeIamPolicy::CreateFromJson(
    std::string const& json_rep) {
  auto json = nlohmann::json::parse(json_rep, nullptr, false);
  // Make sure the json actually parsed successfully
  if (json.is_discarded()) {
    std::ostringstream os;
    os << "Invalid IamPolicy payload, it failed to parse as valid JSON. "
          "payload="
       << json_rep;
    return Status(StatusCode::kInvalidArgument, os.str());
  }
  Status status;
  status = IsObjectIfPresent(json, json_rep, "", "top level node");
  if (!status.ok()) {
    return status;
  }
  status = IsIntIfPresent(json, json_rep, "version", "'version' field");
  if (!status.ok()) {
    return status;
  }
  status = IsStringIfPresent(json, json_rep, "etag", "'etag' field");
  if (!status.ok()) {
    return status;
  }
  status = IsArrayIfPresent(json, json_rep, "bindings", "'bindings' field");
  if (!status.ok()) {
    return status;
  }
  std::vector<NativeIamBinding> bindings;
  auto binding_it = json.find("bindings");
  if (binding_it != json.end()) {
    for (auto const& kv : binding_it->items()) {
      auto binding_impl =
          NativeIamBinding::Impl::CreateFromJson(kv.value(), json_rep);
      if (!binding_impl) {
        return binding_impl.status();
      }
      bindings.emplace_back(*std::move(binding_impl));
    }
    json.erase(binding_it);
  }
  // Cannot use make_unique because we are using brace-initialization.
  // NOLINTNEXTLINE(modernize-make-unique)
  return NativeIamPolicy(std::unique_ptr<NativeIamPolicy::Impl>(
      new NativeIamPolicy::Impl{std::move(json), std::move(bindings)}));
}

std::string NativeIamPolicy::ToJson() const { return pimpl_->ToJson().dump(); }

NativeIamPolicy& NativeIamPolicy::operator=(NativeIamPolicy const& other) {
  *pimpl_ = *other.pimpl_;
  return *this;
}

// NOLINTNEXTLINE(readability-identifier-naming)
std::int32_t NativeIamPolicy::version() const {
  return pimpl_->native_json.value("version", 0);
}

// NOLINTNEXTLINE(readability-identifier-naming)
void NativeIamPolicy::set_version(std::int32_t version) {
  pimpl_->native_json["version"] = version;
}

// NOLINTNEXTLINE(readability-identifier-naming)
std::string NativeIamPolicy::etag() const {
  return pimpl_->native_json.value("etag", "");
}

// NOLINTNEXTLINE(readability-identifier-naming)
void NativeIamPolicy::set_etag(std::string etag) {
  pimpl_->native_json["etag"] = std::move(etag);
}

// NOLINTNEXTLINE(readability-identifier-naming)
std::vector<NativeIamBinding>& NativeIamPolicy::bindings() {
  return pimpl_->bindings;
}

// NOLINTNEXTLINE(readability-identifier-naming)
std::vector<NativeIamBinding> const& NativeIamPolicy::bindings() const {
  return pimpl_->bindings;
}

std::ostream& operator<<(std::ostream& os, NativeIamPolicy const& rhs) {
  os << "NativeIamPolicy={version=" << rhs.version() << ", bindings="
     << "NativeIamBindings={";
  bool first = true;
  for (auto const& binding : rhs.bindings()) {
    os << (first ? "" : ", ") << binding;
    first = false;
  }
  return os << "}, etag=" << rhs.etag() << "}";
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
