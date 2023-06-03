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

#include "google/cloud/opentelemetry/resource_detector.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/compute_engine_util.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/rest_client.h"
#include "google/cloud/log.h"
#include <nlohmann/json.hpp>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/sdk/resource/semantic_conventions.h>

namespace google {
namespace cloud {
namespace otel {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace sc = opentelemetry::sdk::resource::SemanticConventions;

// The metadata server returns fully qualified names. (e.g. a zone may be
// "projects/p/zones/us-central1-a"). Return the IDs only.
std::string Tail(std::string const& value) {
  auto const pos = value.rfind('/');
  return value.substr(pos + 1);
}

std::string MD(nlohmann::json const& json,
               std::vector<std::string> const& keys) {
  auto const* j = &json;
  for (auto const& k : keys) {
    if (!j->contains(k)) return {};
    j = &((*j)[k]);
  }
  if (j->is_string()) return j->get<std::string>();
  if (j->is_number()) return std::to_string(j->get<std::int64_t>());
  return {};
}

void SetAttribute(opentelemetry::sdk::resource::ResourceAttributes& attributes,
                  char const* key, std::string const& value) {
  if (value.empty()) return;
  attributes.SetAttribute(key, value);
}

void SetEnvAttribute(
    opentelemetry::sdk::resource::ResourceAttributes& attributes,
    char const* key, char const* env) {
  auto e = internal::GetEnv(env);
  if (!e || e->empty()) return;
  attributes.SetAttribute(key, *e);
}

enum CloudPlatform { kGke, kCloudFunctions, kCloudRun, kGae, kGce, kUnknown };

CloudPlatform DetectCloudPlatform(nlohmann::json const& json) {
  if (internal::GetEnv("KUBERNETES_SERVICE_HOST")) return kGke;
  if (internal::GetEnv("FUNCTION_TARGET")) return kCloudFunctions;
  if (internal::GetEnv("K_CONFIGURATION")) return kCloudRun;
  if (internal::GetEnv("GAE_SERVICE")) return kGae;
  if (!MD(json, {"instance", "machineType"}).empty()) return kGce;
  return kUnknown;
}

void Gke(opentelemetry::sdk::resource::ResourceAttributes& attributes,
         nlohmann::json const& json) {
  attributes.SetAttribute(sc::kCloudPlatform, "gcp_kubernetes_engine");
  SetAttribute(attributes, sc::kK8sClusterName,
               MD(json, {"instance", "attributes", "cluster-name"}));
  SetAttribute(attributes, sc::kHostId, MD(json, {"instance", "id"}));
  auto cluster_location =
      Tail(MD(json, {"instance", "attributes", "cluster-location"}));

  // The cluster location is either a region (us-west1) or a zone (us-west1-a)
  auto hyphen_count =
      std::count(cluster_location.begin(), cluster_location.end(), '-');
  if (hyphen_count == 1) {
    attributes.SetAttribute(sc::kCloudRegion, cluster_location);
  } else if (hyphen_count == 2) {
    attributes.SetAttribute(sc::kCloudAvailabilityZone, cluster_location);
  }
}

void CloudFunctions(
    opentelemetry::sdk::resource::ResourceAttributes& attributes,
    nlohmann::json const& json) {
  attributes.SetAttribute(sc::kCloudPlatform, "gcp_cloud_functions");
  SetEnvAttribute(attributes, sc::kFaasName, "K_SERVICE");
  SetEnvAttribute(attributes, sc::kFaasVersion, "K_REVISION");
  SetAttribute(attributes, sc::kFaasInstance, MD(json, {"instance", "id"}));
}

void CloudRun(opentelemetry::sdk::resource::ResourceAttributes& attributes,
              nlohmann::json const& json) {
  attributes.SetAttribute(sc::kCloudPlatform, "gcp_cloud_run");
  SetEnvAttribute(attributes, sc::kFaasName, "K_SERVICE");
  SetEnvAttribute(attributes, sc::kFaasVersion, "K_REVISION");
  SetAttribute(attributes, sc::kFaasInstance, MD(json, {"instance", "id"}));
}

void Gae(opentelemetry::sdk::resource::ResourceAttributes& attributes,
         nlohmann::json const& json) {
  attributes.SetAttribute(sc::kCloudPlatform, "gcp_app_engine");
  SetEnvAttribute(attributes, sc::kFaasName, "GAE_SERVICE");
  SetEnvAttribute(attributes, sc::kFaasVersion, "GAE_VERSION");
  SetEnvAttribute(attributes, sc::kFaasInstance, "GAE_INSTANCE");

  auto zone = Tail(MD(json, {"instance", "zone"}));
  SetAttribute(attributes, sc::kCloudAvailabilityZone, zone);
  auto const pos = zone.rfind('-');
  SetAttribute(attributes, sc::kCloudRegion, zone.substr(0, pos));
}

void Gce(opentelemetry::sdk::resource::ResourceAttributes& attributes,
         nlohmann::json const& json) {
  attributes.SetAttribute(sc::kCloudPlatform, "gcp_compute_engine");
  SetAttribute(attributes, sc::kHostType,
               Tail(MD(json, {"instance", "machineType"})));
  SetAttribute(attributes, sc::kHostId, MD(json, {"instance", "id"}));
  SetAttribute(attributes, sc::kHostName, MD(json, {"instance", "name"}));

  auto zone = Tail(MD(json, {"instance", "zone"}));
  attributes.SetAttribute(sc::kCloudAvailabilityZone, zone);
  auto const pos = zone.rfind('-');
  attributes.SetAttribute(sc::kCloudRegion, zone.substr(0, pos));
}

class ResourceDetector final
    : public opentelemetry::sdk::resource::ResourceDetector {
 public:
  explicit ResourceDetector(otel_internal::HttpClientFactory factory,
                            Options options)
      : client_factory_(std::move(factory)), options_(std::move(options)) {}

  opentelemetry::sdk::resource::Resource Detect() override {
    auto client = client_factory_(options_);
    rest_internal::RestRequest request;
    request.SetPath(absl::StrCat(internal::GceMetadataScheme(), "://",
                                 internal::GceMetadataHostname(),
                                 "/computeMetadata/v1/"));
    request.AddHeader("metadata-flavor", "Google");
    request.AddQueryParameter("recursive", "true");
    rest_internal::RestContext context;
    auto response = client->Get(context, request);

    auto log_error = [](Status const& status) {
      GCP_LOG(INFO) << "Could not query the metadata server. status=" << status;
      return opentelemetry::sdk::resource::Resource::GetEmpty();
    };

    if (!response) {
      auto status = std::move(response).status();
      if (status.code() == StatusCode::kUnavailable) {
        // This is most likely a "curl: (6) Could not resolve host" error, which
        // means the application is not running on GCP.
        return opentelemetry::sdk::resource::Resource::GetEmpty();
      }
      return log_error(status);
    }
    if (!response) return log_error(std::move(response).status());
    if (rest_internal::IsHttpError(**response)) {
      auto status = rest_internal::AsStatus(std::move(**response));
      return log_error(status);
    }

    auto payload =
        rest_internal::ReadAll(std::move(**response).ExtractPayload());
    if (!payload.ok()) return log_error(payload.status());
    auto json = nlohmann::json::parse(*payload, nullptr, false);

    opentelemetry::sdk::resource::ResourceAttributes attributes;
    attributes.SetAttribute(sc::kCloudProvider, "gcp");
    SetAttribute(attributes, sc::kCloudAccountId,
                 MD(json, {"project", "projectId"}));

    switch (DetectCloudPlatform(json)) {
      case kGke:
        Gke(attributes, json);
        break;
      case kCloudFunctions:
        CloudFunctions(attributes, json);
        break;
      case kCloudRun:
        CloudRun(attributes, json);
        break;
      case kGae:
        Gae(attributes, json);
        break;
      case kGce:
        Gce(attributes, json);
        break;
      default:
        break;
    }

    return opentelemetry::sdk::resource::Resource::Create(attributes);
  }

 private:
  otel_internal::HttpClientFactory client_factory_;
  Options options_;
};

}  // namespace

std::unique_ptr<opentelemetry::sdk::resource::ResourceDetector>
MakeResourceDetector(Options options) {
  return otel_internal::MakeResourceDetector(
      [](Options const& options) {
        return rest_internal::MakeDefaultRestClient("", options);
      },
      std::move(options));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel
namespace otel_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::unique_ptr<opentelemetry::sdk::resource::ResourceDetector>
MakeResourceDetector(HttpClientFactory factory, Options options) {
  return std::make_unique<otel::ResourceDetector>(std::move(factory),
                                                  std::move(options));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel_internal
}  // namespace cloud
}  // namespace google
