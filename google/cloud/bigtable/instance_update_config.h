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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INSTANCE_UPDATE_CONFIG_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INSTANCE_UPDATE_CONFIG_H

#include "google/cloud/bigtable/cluster_config.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/internal/algorithm.h"
#include <google/bigtable/admin/v2/bigtable_instance_admin.pb.h>
#include <google/bigtable/admin/v2/common.pb.h>
#include <map>
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
using Instance = ::google::bigtable::admin::v2::Instance;

/// Specify the initial configuration for updating an instance.
class InstanceUpdateConfig {
 public:
  // NOLINTNEXTLINE(google-explicit-constructor)
  InstanceUpdateConfig(Instance instance) {
    proto_.mutable_instance()->Swap(&instance);
  }

  //@{
  /// @name Convenient shorthands for the instance types.
  using InstanceType = ::google::bigtable::admin::v2::Instance::Type;
  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr static InstanceType TYPE_UNSPECIFIED =
      google::bigtable::admin::v2::Instance::TYPE_UNSPECIFIED;
  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr static InstanceType PRODUCTION =
      google::bigtable::admin::v2::Instance::PRODUCTION;
  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr static InstanceType DEVELOPMENT =
      google::bigtable::admin::v2::Instance::DEVELOPMENT;
  //@}

  //@{
  /// @name Convenient shorthands for the instance state.
  using StateType = ::google::bigtable::admin::v2::Instance::State;
  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr static StateType STATE_NOT_KNOWN =
      google::bigtable::admin::v2::Instance::STATE_NOT_KNOWN;
  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr static StateType READY =
      google::bigtable::admin::v2::Instance::READY;
  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr static StateType CREATING =
      google::bigtable::admin::v2::Instance::CREATING;
  //@}

  InstanceUpdateConfig& set_type(InstanceType type) {
    proto_.mutable_instance()->set_type(std::move(type));
    AddPathIfNotPresent("type");
    return *this;
  }

  InstanceUpdateConfig& set_state(StateType state) {
    proto_.mutable_instance()->set_state(std::move(state));
    AddPathIfNotPresent("state");
    return *this;
  }

  InstanceUpdateConfig& set_name(std::string name) {
    proto_.mutable_instance()->set_name(std::move(name));
    AddPathIfNotPresent("name");
    return *this;
  }

  InstanceUpdateConfig& set_display_name(std::string display_name) {
    proto_.mutable_instance()->set_display_name(std::move(display_name));
    AddPathIfNotPresent("display_name");
    return *this;
  }

  InstanceUpdateConfig& insert_label(std::string const& key,
                                     std::string const& value) {
    (*proto_.mutable_instance()->mutable_labels())[key] = value;
    AddPathIfNotPresent("labels");
    return *this;
  }

  InstanceUpdateConfig& emplace_label(std::string const& key,
                                      std::string value) {
    (*proto_.mutable_instance()->mutable_labels())[key] = std::move(value);
    AddPathIfNotPresent("labels");
    return *this;
  }

  // NOLINT: accessors can (and should) be snake_case.
  google::bigtable::admin::v2::PartialUpdateInstanceRequest const& as_proto()
      const& {
    return proto_;
  }

  // NOLINT: accessors can (and should) be snake_case.
  google::bigtable::admin::v2::PartialUpdateInstanceRequest&& as_proto() && {
    return std::move(proto_);
  }

  std::string const& GetName() { return proto_.mutable_instance()->name(); }

 private:
  void AddPathIfNotPresent(std::string const& field_name) {
    if (!google::cloud::internal::Contains(proto_.update_mask().paths(),
                                           field_name)) {
      proto_.mutable_update_mask()->add_paths(field_name);
    }
  }

  google::bigtable::admin::v2::PartialUpdateInstanceRequest proto_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INSTANCE_UPDATE_CONFIG_H
