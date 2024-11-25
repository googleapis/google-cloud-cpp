// Copyright 2023 Google LLC
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

#include "google/cloud/opentelemetry/internal/monitored_resource.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include <opentelemetry/common/attribute_value.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/sdk/resource/semantic_conventions.h>
#include <numeric>
#include <unordered_map>

namespace google {
namespace cloud {
namespace otel_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace sc = opentelemetry::sdk::resource::SemanticConventions;

struct AsStringVisitor {
  template <typename T>
  std::string operator()(std::vector<T> const& v) const {
    return absl::StrCat("[",
                        absl::StrJoin(v, ", ",
                                      [this](std::string* out, T const& v) {
                                        out->append(this->operator()(v));
                                      }),
                        "]");
  }
  template <typename T>
  std::string operator()(T const& v) const {
    return absl::StrCat(v);
  }
  std::string operator()(bool const& v) const { return v ? "true" : "false"; }
};

class MonitoredResourceProvider {
 public:
  MonitoredResourceProvider(
      std::string type, std::unordered_map<std::string, OTelKeyMatch> lookup)
      : type_(std::move(type)), lookup_(std::move(lookup)) {}

  MonitoredResource Process(
      opentelemetry::sdk::resource::ResourceAttributes const& attributes) {
    MonitoredResource mr;
    mr.type = type_;
    for (auto const& kv : lookup_) {
      auto const& oks = kv.second.otel_keys;
      auto found = std::find_first_of(
          oks.begin(), oks.end(), attributes.begin(), attributes.end(),
          [](auto const& key, auto const& attr) { return key == attr.first; });
      if (found != oks.end()) {
        mr.labels[kv.first] = AsString(attributes.at(*found));
      } else if (kv.second.fallback) {
        mr.labels[kv.first] = *kv.second.fallback;
      }
    }
    return mr;
  }

 private:
  std::string type_;
  std::unordered_map<std::string, OTelKeyMatch> lookup_;
};

MonitoredResourceProvider GceInstance() {
  return MonitoredResourceProvider("gce_instance",
                                   {
                                       {"zone", {{sc::kCloudAvailabilityZone}}},
                                       {"instance_id", {{sc::kHostId}}},
                                   });
}

MonitoredResourceProvider K8sContainer() {
  return MonitoredResourceProvider(
      "k8s_container",
      {
          {"location", {{sc::kCloudAvailabilityZone, sc::kCloudRegion}}},
          {"cluster_name", {{sc::kK8sClusterName}}},
          {"namespace_name", {{sc::kK8sNamespaceName}}},
          {"pod_name", {{sc::kK8sPodName}}},
          {"container_name", {{sc::kK8sContainerName}}},
      });
}

MonitoredResourceProvider K8sPod() {
  return MonitoredResourceProvider(
      "k8s_pod",
      {
          {"location", {{sc::kCloudAvailabilityZone, sc::kCloudRegion}}},
          {"cluster_name", {{sc::kK8sClusterName}}},
          {"namespace_name", {{sc::kK8sNamespaceName}}},
          {"pod_name", {{sc::kK8sPodName}}},
      });
}

MonitoredResourceProvider K8sNode() {
  return MonitoredResourceProvider(
      "k8s_node",
      {
          {"location", {{sc::kCloudAvailabilityZone, sc::kCloudRegion}}},
          {"cluster_name", {{sc::kK8sClusterName}}},
          {"node_name", {{sc::kK8sNodeName}}},
      });
}

MonitoredResourceProvider K8sCluster() {
  return MonitoredResourceProvider(
      "k8s_cluster",
      {
          {"location", {{sc::kCloudAvailabilityZone, sc::kCloudRegion}}},
          {"cluster_name", {{sc::kK8sClusterName}}},
      });
}

MonitoredResourceProvider GaeInstance() {
  return MonitoredResourceProvider(
      "gae_instance",
      {
          {"location", {{sc::kCloudAvailabilityZone, sc::kCloudRegion}}},
          {"module_id", {{sc::kFaasName}}},
          {"version_id", {{sc::kFaasVersion}}},
          {"instance_id", {{sc::kFaasInstance}}},
      });
}

MonitoredResourceProvider AwsEc2Instance() {
  return MonitoredResourceProvider(
      "aws_ec2_instance",
      {
          {"instance_id", {{sc::kHostId}}},
          {"region", {{sc::kCloudAvailabilityZone, sc::kCloudRegion}}},
          {"aws_account", {{sc::kCloudAccountId}}},
      });
}

MonitoredResourceProvider GenericTask() {
  return MonitoredResourceProvider(
      "generic_task",
      {
          {"location",
           {{sc::kCloudAvailabilityZone, sc::kCloudRegion}, "global"}},
          {"namespace", {{sc::kServiceNamespace}}},
          {"job", {{sc::kServiceName}}},
          {"task_id", {{sc::kServiceInstanceId}}},
      });
}

MonitoredResourceProvider GenericNode() {
  return MonitoredResourceProvider(
      "generic_node",
      {
          {"location",
           {{sc::kCloudAvailabilityZone, sc::kCloudRegion}, "global"}},
          {"namespace", {{sc::kServiceNamespace}}},
          {"node_id", {{sc::kHostId}}},
      });
}

// The resource mapping logic is copied from the go implementation, at:
//
// https://github.com/GoogleCloudPlatform/opentelemetry-operations-go/blob/02fd6f23e8557907cda61ef01c94198dec4ccd71/internal/resourcemapping/resourcemapping.go
MonitoredResourceProvider MakeProvider(
    opentelemetry::sdk::resource::ResourceAttributes const& attributes) {
  std::string platform;
  auto p = attributes.find(sc::kCloudPlatform);
  if (p != attributes.end()) platform = AsString(p->second);

  if (platform == "gcp_compute_engine") {
    return GceInstance();
  }
  if (platform == "gcp_kubernetes_engine") {
    if (attributes.find(sc::kK8sContainerName) != attributes.end()) {
      return K8sContainer();
    }
    if (attributes.find(sc::kK8sPodName) != attributes.end()) {
      return K8sPod();
    }
    if (attributes.find(sc::kK8sNodeName) != attributes.end()) {
      return K8sNode();
    }
    return K8sCluster();
  }
  if (platform == "gcp_app_engine") {
    return GaeInstance();
  }
  if (platform == "aws_ec2") {
    return AwsEc2Instance();
  }
  if ((attributes.find(sc::kServiceName) != attributes.end() &&
       attributes.find(sc::kServiceInstanceId) != attributes.end()) ||
      (attributes.find(sc::kFaasName) != attributes.end() &&
       attributes.find(sc::kFaasInstance) != attributes.end())) {
    return GenericTask();
  }
  return GenericNode();
}

}  // namespace

std::string AsString(
    opentelemetry::sdk::common::OwnedAttributeValue const& attribute) {
  return absl::visit(AsStringVisitor{}, attribute);
}

MonitoredResource ToMonitoredResource(
    opentelemetry::sdk::resource::ResourceAttributes const& attributes) {
  auto provider = MakeProvider(attributes);
  return provider.Process(attributes);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel_internal
}  // namespace cloud
}  // namespace google
