// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "generator/internal/request_id.h"
#include <google/api/field_info.pb.h>

namespace google {
namespace cloud {
namespace generator_internal {

bool MeetsRequestIdRequirements(
    google::protobuf::FieldDescriptor const& descriptor) {
  if (descriptor.type() != google::protobuf::FieldDescriptor::TYPE_STRING ||
      descriptor.is_repeated() || descriptor.has_presence()) {
    return false;
  }
  if (!descriptor.options().HasExtension(google::api::field_info)) return false;
  auto const info = descriptor.options().GetExtension(google::api::field_info);
  return info.format() == google::api::FieldInfo::UUID4;
}

std::string RequestIdFieldName(
    YAML::Node const& service_config,
    google::protobuf::MethodDescriptor const& descriptor) try {
  if (descriptor.input_type() == nullptr) return {};
  auto const& request_descriptor = *descriptor.input_type();
  if (service_config.Type() != YAML::NodeType::Map) return {};
  // This code is fairly defensive. First we need to find the
  // `publishing.method_settings` node, which must be a sequence.
  auto const& publishing = service_config["publishing"];
  if (publishing.Type() != YAML::NodeType::Map) return {};
  auto const& method_settings = publishing["method_settings"];
  if (method_settings.Type() != YAML::NodeType::Sequence) return {};
  for (auto const& m : method_settings) {
    // Each node in the `method_settings` sequence contains a map, the
    // `selector` field in the map is a string that may match the name of the
    // method we are interested in.
    if (m.Type() != YAML::NodeType::Map) continue;
    auto const& selector = m["selector"];
    if (selector.Type() != YAML::NodeType::Scalar) continue;
    if (selector.as<std::string>() != descriptor.full_name()) continue;
    // Once we find the method, we need to find any auto populated field that
    // meets the requirements for a request id.
    auto const& auto_populated = m["auto_populated_fields"];
    if (auto_populated.Type() != YAML::NodeType::Sequence) continue;
    for (auto const& f : auto_populated) {
      if (f.Type() != YAML::NodeType::Scalar) continue;
      auto const* fd = request_descriptor.FindFieldByName(f.as<std::string>());
      if (fd == nullptr) continue;
      if (MeetsRequestIdRequirements(*fd)) return std::string{fd->name()};
    }
  }
  return {};
} catch (YAML::Exception const& ex) {
  // Ignore errors in the YAML file. If it is broken just fallback to having
  // no field.
  return {};
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
