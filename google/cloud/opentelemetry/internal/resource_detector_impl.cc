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

#include "google/cloud/opentelemetry/internal/resource_detector_impl.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/compute_engine_util.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/rest_retry_loop.h"
#include "google/cloud/log.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include <nlohmann/json.hpp>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/sdk/resource/semantic_conventions.h>

namespace google {
namespace cloud {
namespace otel_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace sc = opentelemetry::sdk::resource::SemanticConventions;

// The metadata server returns fully qualified names. (e.g. a zone may be
// "projects/p/zones/us-central1-a"). Return the IDs only.
std::string Tail(std::string const& value) {
  auto const pos = value.rfind('/');
  return value.substr(pos + 1);
}

// NOLINTNEXTLINE(misc-no-recursion);
std::string FindRecursive(nlohmann::json json, std::deque<std::string> keys) {
  if (keys.empty()) {
    if (json.is_string()) return json.get<std::string>();
    if (json.is_number()) return std::to_string(json.get<std::int64_t>());
    return {};
  }
  auto const l = json.find(keys.front());
  if (l == json.end()) return {};
  keys.pop_front();
  return FindRecursive(*l, std::move(keys));
}

bool ValidateHeaders(std::multimap<std::string, std::string> const& headers) {
  auto header = headers.find("content-type");
  if (header == headers.end()) return false;
  if (!absl::StartsWith(header->second, "application/json")) return false;

  header = headers.find("metadata-flavor");
  if (header == headers.end()) return false;
  return absl::AsciiStrToLower(header->second) == "google";
}

bool ValidateJson(nlohmann::json const& json) {
  return json.is_object() && json.find("project") != json.end();
}

// This class is essentially a function that takes in metadata and returns
// resource attributes. We only use a class because it simplifies the code.
class Parser {
 public:
  explicit Parser(nlohmann::json metadata) : metadata_(std::move(metadata)) {
    ProcessMetadataAndEnv();
  }

  opentelemetry::sdk::resource::ResourceAttributes Attributes() && {
    return attributes_;
  }

 private:
  // Synthesize the metadata returned from the metadata server and certain
  // environment variables into resource attributes. This populates the
  // `attributes_` member.
  void ProcessMetadataAndEnv() {
    SetAttribute(sc::kCloudProvider, "gcp");
    SetAttribute(sc::kCloudAccountId, Metadata({"project", "projectId"}));

    if (internal::GetEnv("KUBERNETES_SERVICE_HOST")) { Gke(); return;}
    if (internal::GetEnv("FUNCTION_TARGET")) { CloudFunctions(); return;}
    if (internal::GetEnv("K_CONFIGURATION")) { CloudRun();return;}
    if (internal::GetEnv("GAE_SERVICE"))  {Gae(); return;}
    if (!Metadata({"instance", "machineType"}).empty()) { Gce();return;}
  }

  void Gke() {
    SetAttribute(sc::kCloudPlatform, "gcp_kubernetes_engine");
    SetAttribute(sc::kK8sClusterName,
                 Metadata({"instance", "attributes", "cluster-name"}));
    SetAttribute(sc::kHostId, Metadata({"instance", "id"}));
    auto cluster_location =
        Tail(Metadata({"instance", "attributes", "cluster-location"}));

    // The cluster location is either a region (us-west1) or a zone (us-west1-a)
    auto hyphen_count =
        std::count(cluster_location.begin(), cluster_location.end(), '-');
    if (hyphen_count == 1) {
      SetAttribute(sc::kCloudRegion, cluster_location);
    } else if (hyphen_count == 2) {
      SetAttribute(sc::kCloudAvailabilityZone, cluster_location);
    }
  }

  void CloudFunctions() {
    SetAttribute(sc::kCloudPlatform, "gcp_cloud_functions");
    SetEnvAttribute(sc::kFaasName, "K_SERVICE");
    SetEnvAttribute(sc::kFaasVersion, "K_REVISION");
    SetAttribute(sc::kFaasInstance, Metadata({"instance", "id"}));
  }

  void CloudRun() {
    SetAttribute(sc::kCloudPlatform, "gcp_cloud_run");
    SetEnvAttribute(sc::kFaasName, "K_SERVICE");
    SetEnvAttribute(sc::kFaasVersion, "K_REVISION");
    SetAttribute(sc::kFaasInstance, Metadata({"instance", "id"}));
  }

  void Gae() {
    SetAttribute(sc::kCloudPlatform, "gcp_app_engine");
    SetEnvAttribute(sc::kFaasName, "GAE_SERVICE");
    SetEnvAttribute(sc::kFaasVersion, "GAE_VERSION");
    SetEnvAttribute(sc::kFaasInstance, "GAE_INSTANCE");

    auto zone = Tail(Metadata({"instance", "zone"}));
    SetAttribute(sc::kCloudAvailabilityZone, zone);
    auto const pos = zone.rfind('-');
    SetAttribute(sc::kCloudRegion, zone.substr(0, pos));
  }

  void Gce() {
    SetAttribute(sc::kCloudPlatform, "gcp_compute_engine");
    SetAttribute(sc::kHostType, Tail(Metadata({"instance", "machineType"})));
    SetAttribute(sc::kHostId, Metadata({"instance", "id"}));
    SetAttribute(sc::kHostName, Metadata({"instance", "name"}));

    auto zone = Tail(Metadata({"instance", "zone"}));
    SetAttribute(sc::kCloudAvailabilityZone, zone);
    auto const pos = zone.rfind('-');
    SetAttribute(sc::kCloudRegion, zone.substr(0, pos));
  }

  std::string Metadata(std::deque<std::string> keys) {
    return FindRecursive(metadata_, std::move(keys));
  }

  void SetAttribute(char const* key, std::string const& value) {
    if (value.empty()) return;
    attributes_.SetAttribute(key, value);
  }

  void SetEnvAttribute(char const* key, char const* env) {
    auto e = internal::GetEnv(env);
    if (e) SetAttribute(key, *e);
  }

  nlohmann::json metadata_;
  opentelemetry::sdk::resource::ResourceAttributes attributes_;
};

class GcpResourceDetector
    : public opentelemetry::sdk::resource::ResourceDetector {
 public:
  explicit GcpResourceDetector(HttpClientFactory factory,
                               std::unique_ptr<RetryPolicy> retry,
                               std::unique_ptr<BackoffPolicy> backoff,
                               Options options)
      : client_factory_(std::move(factory)),
        retry_(std::move(retry)),
        backoff_(std::move(backoff)),
        options_(std::move(options)) {
    request_.SetPath(absl::StrCat(internal::GceMetadataScheme(), "://",
                                  internal::GceMetadataHostname(),
                                  "/computeMetadata/v1/"));
    request_.AddHeader("metadata-flavor", "Google");
    request_.AddQueryParameter("recursive", "true");
  }

  opentelemetry::sdk::resource::Resource Detect() override {
    if (attributes_.empty()) {
      auto metadata = QueryMetadataServer();
      if (!metadata) {
        GCP_LOG(INFO) << "Could not query the metadata server. status="
                      << metadata.status();
        return opentelemetry::sdk::resource::Resource::GetEmpty();
      }
      Parser parser(*std::move(metadata));
      attributes_ = std::move(parser).Attributes();
    }
    return opentelemetry::sdk::resource::Resource::Create(attributes_);
  }

 private:
  StatusOr<nlohmann::json> QueryMetadataServer() {
    auto stub_call = [client = client_factory_(options_)](
                         rest_internal::RestContext& context, Options const&,
                         rest_internal::RestRequest const& request)
        -> StatusOr<nlohmann::json> {
      auto response = client->Get(context, request);
      if (!response) return std::move(response).status();
      if (rest_internal::IsHttpError(**response)) {
        return rest_internal::AsStatus(std::move(**response));
      }
      auto headers = (**response).Headers();
      if (!ValidateHeaders(headers)) {
        return internal::NotFoundError(
            "response headers do not match expectation.");
      }
      auto payload =
          rest_internal::ReadAll(std::move(**response).ExtractPayload());
      if (!payload.ok()) return std::move(payload).status();
      auto json = nlohmann::json::parse(*payload, nullptr, false);
      if (!ValidateJson(json)) {
        return internal::UnavailableError(
            "returned payload does not match expectation.");
      }
      return json;
    };

    return rest_internal::RestRetryLoop(retry_->clone(), backoff_->clone(),
                                        Idempotency::kIdempotent, stub_call,
                                        options_, request_, __func__);
  }

  rest_internal::RestRequest request_;
  HttpClientFactory client_factory_;
  std::unique_ptr<RetryPolicy> retry_;
  std::unique_ptr<BackoffPolicy> backoff_;
  Options options_;
  opentelemetry::sdk::resource::ResourceAttributes attributes_;
};

}  // namespace

std::unique_ptr<opentelemetry::sdk::resource::ResourceDetector>
MakeResourceDetector(HttpClientFactory factory,
                     std::unique_ptr<RetryPolicy> retry,
                     std::unique_ptr<BackoffPolicy> backoff, Options options) {
  return std::make_unique<GcpResourceDetector>(
      std::move(factory), std::move(retry), std::move(backoff),
      std::move(options));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel_internal
}  // namespace cloud
}  // namespace google
