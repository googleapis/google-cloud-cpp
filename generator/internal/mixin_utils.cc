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

#include "generator/internal/mixin_utils.h"
#include "generator/internal/codegen_utils.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/log.h"
#include "absl/base/no_destructor.h"
#include "absl/strings/ascii.h"
#include "absl/types/optional.h"
#include <google/protobuf/compiler/code_generator.h>
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using ::google::protobuf::DescriptorPool;
using ::google::protobuf::FileDescriptor;
using ::google::protobuf::MethodDescriptor;
using ::google::protobuf::ServiceDescriptor;

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

std::unordered_map<std::string, std::string> const& GetMixinProtoPathMap() {
  static absl::NoDestructor<std::unordered_map<std::string, std::string>> const
      kMixinProtoPathMap{{
          {"google.cloud.location.Locations",
           "google/cloud/location/locations.proto"},
          {"google.iam.v1.IAMPolicy", "google/iam/v1/iam_policy.proto"},
          {"google.longrunning.Operations",
           "google/longrunning/operations.proto"},
      }};
  return *kMixinProtoPathMap;
}

google::api::HttpRule ParseHttpRule(YAML::detail::iterator_value const& rule) {
  google::api::HttpRule http_rule;
  auto const& body = rule["body"];
  if (body) {
    http_rule.set_body(body.as<std::string>());
  }
  auto const& selector = rule["selector"];
  if (selector) {
    http_rule.set_selector(selector.as<std::string>());
  }

  for (auto const& kv : rule) {
    if (kv.first.Type() != YAML::NodeType::Scalar ||
        kv.second.Type() != YAML::NodeType::Scalar)
      continue;

    auto const rule_key = absl::AsciiStrToLower(kv.first.as<std::string>());
    auto const rule_value = kv.second.as<std::string>();

    if (rule_key == "get") {
      http_rule.set_get(rule_value);
    } else if (rule_key == "post") {
      http_rule.set_post(rule_value);
    } else if (rule_key == "put") {
      http_rule.set_put(rule_value);
    } else if (rule_key == "patch") {
      http_rule.set_patch(rule_value);
    } else if (rule_key == "delete") {
      http_rule.set_delete_(rule_value);
    }
  }
  return http_rule;
}

/**
 * Extract Mixin methods from the YAML file, together with the overwritten http
 * info.
 */
std::unordered_map<std::string, google::api::HttpRule> GetMixinHttpOverrides(
    YAML::Node const& service_config) {
  std::unordered_map<std::string, google::api::HttpRule> http_rules;

  if (service_config.Type() != YAML::NodeType::Map) return http_rules;
  if (!service_config["http"]) return http_rules;

  auto const& http = service_config["http"];
  if (http.Type() != YAML::NodeType::Map) return http_rules;

  auto const& rules = http["rules"];
  if (rules.Type() != YAML::NodeType::Sequence) return http_rules;

  for (auto const& rule : rules) {
    if (rule.Type() != YAML::NodeType::Map) continue;

    auto const& selector = rule["selector"];
    if (selector.Type() != YAML::NodeType::Scalar) continue;

    auto const method_full_name = selector.as<std::string>();
    google::api::HttpRule http_rule = ParseHttpRule(rule);

    if (rule["additional_bindings"]) {
      auto const& additional_bindings = rule["additional_bindings"];
      if (additional_bindings.Type() == YAML::NodeType::Sequence) {
        for (auto const& additional_binding : additional_bindings) {
          if (additional_binding.Type() != YAML::NodeType::Map) continue;
          *http_rule.add_additional_bindings() =
              ParseHttpRule(additional_binding);
        }
      }
    }

    http_rules[method_full_name] = std::move(http_rule);
  }
  return http_rules;
}

/**
 * Get all methods' names from a service.
 */
std::unordered_set<std::string> GetMethodNames(
    ServiceDescriptor const& service) {
  std::unordered_set<std::string> method_names;
  for (int i = 0; i < service.method_count(); ++i) {
    auto const* method = service.method(i);
    method_names.insert(method->name());
  }
  return method_names;
}

}  // namespace

std::vector<std::string> GetMixinProtoPaths(YAML::Node const& service_config) {
  std::vector<std::string> proto_paths;
  if (service_config.Type() != YAML::NodeType::Map) return proto_paths;
  auto const& apis = service_config["apis"];
  if (apis.Type() != YAML::NodeType::Sequence) return proto_paths;
  for (auto const& api : apis) {
    if (api.Type() != YAML::NodeType::Map) continue;
    auto const& name = api["name"];
    if (name.Type() != YAML::NodeType::Scalar) continue;
    auto const package_path = name.as<std::string>();
    auto const& mixin_proto_path_map = GetMixinProtoPathMap();
    auto const it = mixin_proto_path_map.find(package_path);
    if (it == mixin_proto_path_map.end()) continue;
    proto_paths.push_back(it->second);
  }
  return proto_paths;
}

std::vector<std::string> GetMixinProtoPaths(
    std::string const& service_yaml_path) {
  return GetMixinProtoPaths(YAML::LoadFile(service_yaml_path));
}

std::vector<MixinMethod> GetMixinMethods(YAML::Node const& service_config,
                                         ServiceDescriptor const& service) {
  std::vector<MixinMethod> mixin_methods;
  DescriptorPool const* pool = service.file()->pool();
  if (pool == nullptr) {
    GCP_LOG(FATAL) << __FILE__ << ":" << __LINE__
                   << " DescriptorPool doesn't exist for service: "
                   << service.full_name();
  }
  std::unordered_set<std::string> const method_names = GetMethodNames(service);
  auto const mixin_proto_paths = GetMixinProtoPaths(service_config);
  auto const mixin_http_overrides = GetMixinHttpOverrides(service_config);

  for (auto const& mixin_proto_path : mixin_proto_paths) {
    FileDescriptor const* mixin_file = pool->FindFileByName(mixin_proto_path);
    if (mixin_file == nullptr) {
      GCP_LOG(FATAL) << __FILE__ << ":" << __LINE__
                     << " Mixin FileDescriptor is not found for path: "
                     << mixin_proto_path
                     << " in service: " << service.full_name();
    }
    for (int i = 0; i < mixin_file->service_count(); ++i) {
      ServiceDescriptor const* mixin_service = mixin_file->service(i);
      for (int j = 0; j < mixin_service->method_count(); ++j) {
        MethodDescriptor const* mixin_method = mixin_service->method(j);
        auto mixin_method_full_name = mixin_method->full_name();
        // Add the mixin method only if it appears in the http field of YAML
        auto const it = mixin_http_overrides.find(mixin_method_full_name);
        if (it == mixin_http_overrides.end()) continue;

        // If the mixin method name required from YAML appears in the original
        // service proto, ignore the mixin.
        if (method_names.find(mixin_method->name()) != method_names.end())
          continue;

        mixin_methods.push_back(
            {absl::AsciiStrToLower(mixin_service->name()) + "_stub",
             ProtoNameToCppName(mixin_service->full_name()), *mixin_method,
             it->second});
      }
    }
  }
  return mixin_methods;
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
