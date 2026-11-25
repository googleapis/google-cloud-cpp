// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS

#include "google/cloud/storage/internal/grpc/metrics_exporter_options.h"
#include "google/cloud/opentelemetry/monitoring_exporter.h"
#include "google/cloud/storage/internal/grpc/channel_telemetry.h"
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/credentials_impl.h"
#include "google/cloud/internal/invocation_id_generator.h"
#include "google/cloud/internal/oauth2_universe_domain.h"
#include "google/cloud/internal/unified_grpc_credentials.h"
#include "google/cloud/log.h"
#include "google/cloud/status_or.h"
#include "google/cloud/universe_domain_options.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "google/api/monitored_resource.pb.h"
#include <nlohmann/json.hpp>
#include <opentelemetry/semconv/incubating/cloud_attributes.h>
#include <opentelemetry/semconv/incubating/host_attributes.h>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

auto ByName(opentelemetry::sdk::resource::ResourceAttributes const& attributes,
            std::string const& name, std::string default_value) {
  auto const l = attributes.find(name);
  if (l == attributes.end()) return default_value;
  return std::get<std::string>(l->second);
}

template <typename Option>
void Forward(Options const& from, Options& to) {
  if (from.has<Option>()) to.set<Option>(from.get<Option>());
}

// Returns true if @p host is a DNS name: non-empty labels of letters, digits
// and `-`, separated by `.`.
bool IsHostName(std::string_view host) {
  std::vector<std::string_view> const labels = absl::StrSplit(host, '.');
  return std::all_of(labels.begin(), labels.end(), [](std::string_view label) {
    return !label.empty() &&
           std::all_of(label.begin(), label.end(), [](unsigned char c) {
             return absl::ascii_isalnum(c) || c == '-';
           });
  });
}

// Extracts the DNS host from supported Storage gRPC targets (`google-c2p`
// targets and scheme-less `host` or `host:port` endpoints). Returns an empty
// view for other gRPC target formats (such as `unix:` or `dns:` URIs, IP
// literals, or targets with paths).
std::string_view EndpointHost(std::string_view endpoint) {
  for (std::string_view prefix : {kC2pPrefix, kC2pExperimentalPrefix}) {
    if (absl::StartsWith(endpoint, prefix)) {
      std::string_view const rest = endpoint.substr(prefix.size());
      endpoint = rest.substr(0, rest.find('?'));
      break;
    }
  }
  std::string_view::size_type const colon = endpoint.find(':');
  std::string_view const host = endpoint.substr(0, colon);
  if (colon != std::string_view::npos) {
    std::string_view const port = endpoint.substr(colon + 1);
    bool const is_port =
        !port.empty() &&
        std::all_of(port.begin(), port.end(),
                    [](unsigned char c) { return absl::ascii_isdigit(c); });
    if (!is_port) return {};
  }
  if (!IsHostName(host)) return {};
  return host;
}

struct CredentialsSummary {
  bool insecure = false;
  bool from_host = false;
  // False when a credential source carries a universe domain that could not
  // be read or parsed.
  bool universe_domain_known = true;
  std::optional<std::string> universe_domain;
};

CredentialsSummary Summarize(Credentials const& credentials) {
  struct Visitor : public internal::CredentialsVisitor {
    CredentialsSummary summary;

    void UniverseDomainUnknown() { summary.universe_domain_known = false; }

    void FromCredentialsFile(std::string const& contents) {
      nlohmann::json const json =
          nlohmann::json::parse(contents, nullptr, false);
      // `GetUniverseDomainFromCredentialsJson()` expects a JSON object. It
      // reads anything else as a file without the field, that is, as a file
      // for the default universe.
      if (!json.is_object()) return UniverseDomainUnknown();
      StatusOr<std::string> universe_domain =
          oauth2_internal::GetUniverseDomainFromCredentialsJson(json);
      if (!universe_domain) return UniverseDomainUnknown();
      summary.universe_domain = *universe_domain;
    }

    void visit(internal::ErrorCredentialsConfig const&) override {}
    void visit(internal::InsecureCredentialsConfig const&) override {
      summary.insecure = true;
    }
    void visit(internal::GoogleDefaultCredentialsConfig const&) override {
      StatusOr<internal::AdcLookup> adc =
          internal::LookupApplicationDefaultCredentials();
      if (!adc) return UniverseDomainUnknown();
      switch (adc->source) {
        case internal::AdcSource::kNone:
          summary.from_host = true;
          return;
        case internal::AdcSource::kServiceAccount:
        case internal::AdcSource::kAuthorizedUser:
        case internal::AdcSource::kExternalAccount:
          return FromCredentialsFile(*adc->contents);
        case internal::AdcSource::kNotJsonObject:
        case internal::AdcSource::kUnsupportedType:
          // gRPC's Google Default credentials do not accept these files, so
          // this library cannot tell which credentials the channel uses.
          return UniverseDomainUnknown();
      }
    }
    void visit(internal::AccessTokenConfig const&) override {}
    void visit(internal::ImpersonateServiceAccountConfig const& cfg) override {
      // Impersonated credentials are obtained with the base credentials, so
      // the base credentials decide which universe they are for, and whether
      // they come from the host.
      std::shared_ptr<Credentials> const base = cfg.base_credentials();
      if (!base) return UniverseDomainUnknown();
      internal::CredentialsVisitor::dispatch(*base, *this);
    }
    void visit(internal::ServiceAccountConfig const& cfg) override {
      if (cfg.json_object().has_value()) {
        return FromCredentialsFile(*cfg.json_object());
      }
      // A file that cannot be read yields an empty string, which is not a
      // JSON object.
      std::ifstream is(cfg.file_path().value_or(""));
      FromCredentialsFile(std::string{std::istreambuf_iterator<char>{is}, {}});
    }
    void visit(internal::ExternalAccountConfig const& cfg) override {
      FromCredentialsFile(cfg.json_object());
    }
    void visit(internal::ApiKeyConfig const&) override {}
    void visit(internal::ComputeEngineCredentialsConfig const&) override {
      summary.from_host = true;
    }
    void visit(internal::AuthorizedUserConfig const& cfg) override {
      FromCredentialsFile(cfg.json_object());
    }
    void visit(internal::GDCHServiceAccountConfig const&) override {}
  };

  Visitor visitor;
  internal::CredentialsVisitor::dispatch(credentials, visitor);
  return visitor.summary;
}

}  // namespace

Options MetricsExporterOptions(
    Project const& project,
    opentelemetry::sdk::resource::Resource const& resource) {
  namespace sc = ::opentelemetry::semconv;

  auto const& attributes = resource.GetAttributes();
  auto monitored_resource = google::api::MonitoredResource{};
  monitored_resource.set_type("storage.googleapis.com/Client");
  auto& labels = *monitored_resource.mutable_labels();
  labels["project_id"] = project.project_id();
  labels["location"] =
      ByName(attributes, sc::cloud::kCloudAvailabilityZone,
             ByName(attributes, sc::cloud::kCloudRegion, "global"));
  labels["cloud_platform"] =
      ByName(attributes, sc::cloud::kCloudPlatform, "unknown");
  labels["host_id"] = ByName(attributes, "faas.id",
                             ByName(attributes, sc::host::kHostId, "unknown"));

  // We need a UUID because there may be multiple monitored resources within the
  // same process, and we need these monitored resources to be globally unique
  // or GCM may complain about the publication rate. There is no information
  // available that can make this unique enough, all the clients in a service
  // may be using the same project and bucket (not that buckets are available).
  //
  // This is fairly expensive, it requires initializing a new PRNG, and fetching
  // entropy from the OS. Outside tests, this function will be called a handful
  // of times, so the performance is not that important.
  labels["instance_id"] =
      google::cloud::internal::InvocationIdGenerator().MakeInvocationId();
  labels["api"] = "GRPC";

  return Options{}
      .set<otel::ServiceTimeSeriesOption>(true)
      .set<otel::MetricNameFormatterOption>([](std::string s) -> std::string {
        if (s == "gcp.client.request.duration") {
          return "storage.googleapis.com/client/request_duration";
        }
        // Storage SDK metrics under `gcp.storage.client.*` are registered in
        // Cloud Monitoring with flat underscore-separated names rather than
        // slash-separated paths.
        constexpr std::string_view kStorageClientPrefix = "gcp.storage.client.";
        if (absl::StartsWith(s, kStorageClientPrefix)) {
          s.erase(0, kStorageClientPrefix.size());
          std::replace(s.begin(), s.end(), '.', '_');
          return "storage.googleapis.com/client/" + std::move(s);
        }
        std::replace(s.begin(), s.end(), '.', '/');
        if (absl::StartsWith(s, kInternalMetricPrefix)) {
          return "storage.googleapis.com/" + std::move(s);
        }
        return "storage.googleapis.com/client/" + std::move(s);
      })
      .set<otel::MonitoredResourceOption>(std::move(monitored_resource));
}

Options MetricsExporterConnectionOptions(Options const& options) {
  auto result = Options{};
  Forward<UnifiedCredentialsOption>(options, result);
  Forward<GrpcCredentialOption>(options, result);
  Forward<CARootsFilePathOption>(options, result);
  Forward<ProxyOption>(options, result);
  auto ep_canonical = std::string{"storage.googleapis.com"};
  auto ep_private = std::string{"private.googleapis.com"};
  auto ep_restricted = std::string{"restricted.googleapis.com"};
  if (options.has<internal::UniverseDomainOption>()) {
    auto const& ud = options.get<internal::UniverseDomainOption>();
    result.set<internal::UniverseDomainOption>(ud);
    ep_canonical = std::string{"storage."} + ud;
    ep_private = std::string{"private."} + ud;
    ep_restricted = std::string{"restricted."} + ud;
  }
  if (!options.has<EndpointOption>()) return result;
  auto const& ep = options.get<EndpointOption>();
  auto matches = [&ep](auto const& candidate) {
    return ep == candidate || ep == (std::string{"google-c2p:///"} + candidate);
  };
  if (matches(ep_canonical)) return result;
  if (matches(ep_private)) result.set<EndpointOption>(ep_private);
  if (matches(ep_restricted)) result.set<EndpointOption>(ep_restricted);
  return result;
}

MetricsExportMode DetectMetricsExportMode(Options const& options) {
  std::string const universe_domain =
      options.has<internal::UniverseDomainOption>()
          ? options.get<internal::UniverseDomainOption>()
          : oauth2_internal::GoogleDefaultUniverseDomain();
  std::string_view const host = EndpointHost(options.get<EndpointOption>());
  // Require a dot-delimited subdomain of `universe_domain` (for example,
  // `storage.googleapis.com` in `googleapis.com`).
  if (!absl::EndsWithIgnoreCase(host, absl::StrCat(".", universe_domain))) {
    GCP_LOG(INFO) << "gRPC metrics export is disabled: the endpoint is not in"
                     " the universe domain, endpoint_host="
                  << (host.empty() ? "(unrecognized)" : host)
                  << ", universe_domain=" << universe_domain;
    return MetricsExportMode::kDisabled;
  }

  std::shared_ptr<Credentials> const& credentials =
      options.get<UnifiedCredentialsOption>();
  // Only `GrpcCredentialOption` is set. gRPC channel credentials are opaque,
  // so nothing ties them to the host.
  if (!credentials) return MetricsExportMode::kWithoutHostResource;
  CredentialsSummary const summary = Summarize(*credentials);
  if (summary.insecure) {
    GCP_LOG(INFO) << "gRPC metrics export is disabled: the client uses"
                     " insecure credentials";
    return MetricsExportMode::kDisabled;
  }
  if (!summary.universe_domain_known) {
    GCP_LOG(INFO) << "gRPC metrics export is disabled: cannot determine the"
                     " universe domain of the credentials, universe_domain="
                  << universe_domain;
    return MetricsExportMode::kDisabled;
  }
  if (summary.universe_domain.has_value() &&
      !absl::EqualsIgnoreCase(*summary.universe_domain, universe_domain)) {
    GCP_LOG(INFO)
        << "gRPC metrics export is disabled: the credentials are"
           " for another universe domain, credentials_universe_domain="
        << *summary.universe_domain << ", universe_domain=" << universe_domain;
    return MetricsExportMode::kDisabled;
  }
  return summary.from_host ? MetricsExportMode::kWithHostResource
                           : MetricsExportMode::kWithoutHostResource;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
