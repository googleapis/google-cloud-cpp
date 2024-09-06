#include "generator/internal/mixin_utils.h"
#include "generator/internal/codegen_utils.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/log.h"
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

std::unordered_map<std::string, std::string> const kMixinProtoPathMap = {
    {"google.cloud.location.Locations",
     "google/cloud/location/locations.proto"},
    {"google.iam.v1.IAMPolicy", "google/iam/v1/iam_policy.proto"},
    {"google.longrunning.Operations", "google/longrunning/operations.proto"},
};

std::unordered_map<std::string, std::string> const kHttpVerbs = {
    {"get", "Get"},     {"post", "Post"},     {"put", "Put"},
    {"patch", "Patch"}, {"delete", "Delete"},
};

/**
 * Extract Mixin methods from the YAML file, together with the overwritten http
 * info.
 */
std::unordered_map<std::string, MixinMethodOverride> GetMixinMethodOverrides(
    YAML::Node const& service_config) {
  std::unordered_map<std::string, MixinMethodOverride> mixin_method_overrides;
  if (service_config.Type() != YAML::NodeType::Map)
    return mixin_method_overrides;
  if (!service_config["http"]) return mixin_method_overrides;
  auto const& http = service_config["http"];
  if (http.Type() != YAML::NodeType::Map) return mixin_method_overrides;
  auto const& rules = http["rules"];
  if (rules.Type() != YAML::NodeType::Sequence) return mixin_method_overrides;
  for (auto const& rule : rules) {
    if (rule.Type() != YAML::NodeType::Map) continue;

    auto const& selector = rule["selector"];
    if (selector.Type() != YAML::NodeType::Scalar) continue;
    std::string const& method_full_name = selector.as<std::string>();

    for (auto const& kv : rule) {
      if (kv.first.Type() != YAML::NodeType::Scalar ||
          kv.second.Type() != YAML::NodeType::Scalar)
        continue;

      std::string const& http_verb_lower =
          absl::AsciiStrToLower(kv.first.as<std::string>());
      if (kHttpVerbs.find(http_verb_lower) == kHttpVerbs.end()) continue;
      std::string const& http_verb = kHttpVerbs.at(http_verb_lower);
      std::string const& http_path = kv.second.as<std::string>();
      absl::optional<std::string> http_body;
      if (rule["body"]) {
        http_body = rule["body"].as<std::string>();
      }

      mixin_method_overrides[method_full_name] =
          MixinMethodOverride{http_verb, http_path, http_body};
    }
  }
  return mixin_method_overrides;
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
    std::string const& package_path = name.as<std::string>();
    if (kMixinProtoPathMap.find(package_path) == kMixinProtoPathMap.end())
      continue;
    proto_paths.push_back(kMixinProtoPathMap.at(package_path));
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
  auto const mixin_method_overrides = GetMixinMethodOverrides(service_config);

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
        if (mixin_method_overrides.find(mixin_method_full_name) ==
            mixin_method_overrides.end())
          continue;

        // if the mixin method name required from YAML appears in the original
        // service proto, ignore the mixin.
        if (method_names.find(mixin_method->name()) != method_names.end())
          continue;

        mixin_methods.push_back(
            {absl::AsciiStrToLower(mixin_service->name()) + "_stub",
             ProtoNameToCppName(mixin_service->full_name()), *mixin_method,
             mixin_method_overrides.at(mixin_method_full_name)});
      }
    }
  }
  return mixin_methods;
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
