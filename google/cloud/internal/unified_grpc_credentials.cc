// Copyright 2021 Google LLC
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

#include "google/cloud/internal/unified_grpc_credentials.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/filesystem.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/grpc_access_token_authentication.h"
#include "google/cloud/internal/grpc_api_key_authentication.h"
#include "google/cloud/internal/grpc_channel_credentials_authentication.h"
#include "google/cloud/internal/grpc_compute_engine_authentication.h"
#include "google/cloud/internal/grpc_impersonate_service_account.h"
#include "google/cloud/internal/grpc_service_account_authentication.h"
#include "google/cloud/internal/make_status.h"
#include "absl/strings/str_cat.h"
#include <grpcpp/security/credentials.h>
#if __has_include(<grpcpp/version_info.h>)
#include <grpcpp/version_info.h>
#endif
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace {

// Put this outside our own namespace to avoid conflicts when using a naked
// `ExternalAccountCredentials()` call.
std::shared_ptr<grpc::CallCredentials> GrpcExternalAccountCredentials(
    google::cloud::internal::ExternalAccountConfig const& cfg) {
  // The using directives are a deviation from the Google Style Guide. The
  // `ExternalAccountCredentials()` function appeared in gRPC 1.35, in the
  // `grpc::experimental` namespace. It was moved to the `grpc` namespace in
  // gRPC 1.36. We support gRPC >= 1.35. gRPC does not offer version macros
  // until gRPC 1.51. We think the minor deviation from the style guide is
  // justified in this case.
  using namespace ::grpc;                // NOLINT(google-build-using-namespace)
  using namespace ::grpc::experimental;  // NOLINT(google-build-using-namespace)
  return ExternalAccountCredentials(
      cfg.json_object(), cfg.options().get<google::cloud::ScopesOption>());
}

}  // namespace

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

class GrpcErrorCredentialsAuthentication : public GrpcAuthenticationStrategy {
 public:
  explicit GrpcErrorCredentialsAuthentication(ErrorCredentialsConfig const& cfg)
      : error_status_(std::move(cfg.status())) {}
  ~GrpcErrorCredentialsAuthentication() override = default;

  std::shared_ptr<grpc::Channel> CreateChannel(
      std::string const&, grpc::ChannelArguments const&) override {
    return grpc::CreateCustomChannel("error:///",
                                     grpc::InsecureChannelCredentials(), {});
  }
  bool RequiresConfigureContext() const override { return true; }
  Status ConfigureContext(grpc::ClientContext&) override {
    return error_status_;
  }
  future<StatusOr<std::shared_ptr<grpc::ClientContext>>> AsyncConfigureContext(
      std::shared_ptr<grpc::ClientContext>) override {
    return make_ready_future<StatusOr<std::shared_ptr<grpc::ClientContext>>>(
        error_status_);
  }

 private:
  Status error_status_;
};

namespace {

// The lookup below mirrors `oauth2_internal::GoogleDefaultCredentials()`. The
// REST helpers it would reuse are compiled into `rest_internal`, which depends
// on libcurl; `grpc_utils` does not link that library, so the lookup is
// duplicated here rather than adding the dependency.
std::string AdcFilePathFromEnvVarOrEmpty() {
  return GetEnv("GOOGLE_APPLICATION_CREDENTIALS").value_or("");
}

std::string AdcFilePathFromWellKnownPathOrEmpty() {
  std::optional<std::string> const override_path =
      GetEnv("GOOGLE_GCLOUD_ADC_PATH_OVERRIDE");
  if (override_path.has_value()) return *override_path;
#ifdef _WIN32
  std::optional<std::string> const root = GetEnv("APPDATA");
  constexpr std::string_view kSuffix =
      "/gcloud/application_default_credentials.json";
#else
  std::optional<std::string> const root = GetEnv("HOME");
  constexpr std::string_view kSuffix =
      "/.config/gcloud/application_default_credentials.json";
#endif
  if (!root.has_value()) return "";
  return absl::StrCat(*root, kSuffix);
}

// gRPC's Google Default credentials accept a fixed set of `type` values; each
// of its credential factories checks the field again and returns null for
// anything else.
AdcLookup ClassifyAdcFile(std::string path, std::string contents) {
  AdcLookup lookup;
  lookup.path = std::move(path);
  lookup.contents = std::move(contents);
  nlohmann::json const json =
      nlohmann::json::parse(lookup.contents, nullptr, false);
  if (json.is_discarded() || !json.is_object()) {
    lookup.source = AdcSource::kNotJsonObject;
    return lookup;
  }
  auto const type = json.find("type");
  if (type != json.end() && type->is_string()) {
    lookup.type = type->get<std::string>();
  }
  if (lookup.type == "service_account") {
    lookup.source = AdcSource::kServiceAccount;
  } else if (lookup.type == "authorized_user") {
    lookup.source = AdcSource::kAuthorizedUser;
  } else if (lookup.type == "external_account") {
    lookup.source = AdcSource::kExternalAccount;
  } else {
    lookup.source = AdcSource::kUnsupportedType;
  }
  return lookup;
}

std::shared_ptr<GrpcAuthenticationStrategy> ErrorStrategy(Status status) {
  return std::make_shared<GrpcErrorCredentialsAuthentication>(
      ErrorCredentialsConfig{std::move(status)});
}

// Composes TLS channel credentials with channel-level call credentials.
// gRPC's call-credential factories return nullptr on malformed input, and
// `grpc::CompositeChannelCredentials()` dereferences its argument without
// checking for null, so we validate @p call_creds first. @p factory names the
// gRPC factory for the error message.
std::shared_ptr<GrpcAuthenticationStrategy> TlsWithCallCredentials(
    Options const& options,
    std::shared_ptr<grpc::CallCredentials> const& call_creds,
    std::string_view factory, std::string const& path) {
  if (!call_creds) {
    return ErrorStrategy(InvalidArgumentError(
        absl::StrCat("Error creating ", factory,
                     " for Application Default Credentials",
                     path.empty() ? "" : " from ", path),
        GCP_ERROR_INFO()));
  }
  grpc::SslCredentialsOptions ssl_options;
  std::optional<std::string> cainfo = LoadCAInfo(options);
  if (cainfo) ssl_options.pem_root_certs = std::move(*cainfo);
  return std::make_shared<GrpcChannelCredentialsAuthentication>(
      grpc::CompositeChannelCredentials(grpc::SslCredentials(ssl_options),
                                        call_creds));
}

// The strategy for `GrpcGoogleDefaultTlsOnlyOption`: the call credentials
// gRPC's `GoogleDefaultCredentials()` derives from Application Default
// Credentials (see `create_default_creds_from_path()` in gRPC), composed with
// TLS instead of letting gRPC choose between TLS and ALTS per endpoint. The
// error codes and messages follow the REST ADC loader.
std::shared_ptr<GrpcAuthenticationStrategy> MakeTlsGoogleDefaultStrategy(
    GoogleDefaultCredentialsConfig const& cfg, Options const& options) {
  StatusOr<AdcLookup> adc = LookupApplicationDefaultCredentials();
  if (!adc) return ErrorStrategy(std::move(adc).status());
  switch (adc->source) {
    case AdcSource::kNone:
      return TlsWithCallCredentials(
          options, grpc::GoogleComputeEngineCredentials(),
          "grpc::GoogleComputeEngineCredentials", adc->path);
    case AdcSource::kServiceAccount:
      return TlsWithCallCredentials(
          options, grpc::ServiceAccountJWTAccessCredentials(adc->contents),
          "grpc::ServiceAccountJWTAccessCredentials", adc->path);
    case AdcSource::kAuthorizedUser:
      return TlsWithCallCredentials(
          options, grpc::GoogleRefreshTokenCredentials(adc->contents),
          "grpc::GoogleRefreshTokenCredentials", adc->path);
    case AdcSource::kExternalAccount:
      return TlsWithCallCredentials(
          options,
          GrpcExternalAccountCredentials(
              ExternalAccountConfig(adc->contents, cfg.options())),
          "grpc::ExternalAccountCredentials", adc->path);
    case AdcSource::kNotJsonObject:
      return ErrorStrategy(InvalidArgumentError(
          absl::StrCat("Cannot open credentials file ", adc->path,
                       ", it does not contain a JSON object."),
          GCP_ERROR_INFO()));
    case AdcSource::kUnsupportedType:
      break;
  }
  return ErrorStrategy(InvalidArgumentError(
      absl::StrCat("Unsupported credential type (",
                   adc->type.empty() ? "no type given" : adc->type,
                   ") when reading Application Default Credentials file from ",
                   adc->path, "."),
      GCP_ERROR_INFO()));
}

}  // namespace

StatusOr<AdcLookup> LookupApplicationDefaultCredentials() {
  std::string path = AdcFilePathFromEnvVarOrEmpty();
  if (path.empty()) {
    path = AdcFilePathFromWellKnownPathOrEmpty();
    std::error_code ec;
    if (path.empty() || !exists(status(path, ec))) return AdcLookup{};
  }
  std::ifstream is(path);
  if (!is.is_open()) {
    // We use kUnknown here because we don't know if the file does not exist,
    // or if we were unable to open it for some other reason.
    return UnknownError("Cannot open credentials file " + path,
                        GCP_ERROR_INFO());
  }
  std::string contents(std::istreambuf_iterator<char>{is}, {});
  return ClassifyAdcFile(std::move(path), std::move(contents));
}

std::shared_ptr<GrpcAuthenticationStrategy> CreateAuthenticationStrategy(
    google::cloud::CompletionQueue cq, Options const& options) {
  if (options.has<google::cloud::UnifiedCredentialsOption>()) {
    return google::cloud::internal::CreateAuthenticationStrategy(
        *options.get<google::cloud::UnifiedCredentialsOption>(), std::move(cq),
        options);
  }
  return google::cloud::internal::CreateAuthenticationStrategy(
      options.get<google::cloud::GrpcCredentialOption>());
}

std::shared_ptr<GrpcAuthenticationStrategy> CreateAuthenticationStrategy(
    Credentials const& credentials, CompletionQueue cq, Options options) {
  struct Visitor : public CredentialsVisitor {
    CompletionQueue cq;
    Options options;
    std::shared_ptr<GrpcAuthenticationStrategy> result;

    Visitor(CompletionQueue c, Options o)
        : cq(std::move(c)), options(std::move(o)) {}

    void visit(ErrorCredentialsConfig const& cfg) override {
      result = std::make_unique<GrpcErrorCredentialsAuthentication>(cfg);
    }
    void visit(InsecureCredentialsConfig const&) override {
      result = std::make_unique<GrpcChannelCredentialsAuthentication>(
          grpc::InsecureChannelCredentials());
    }
    void visit(GoogleDefaultCredentialsConfig const& cfg) override {
      if (options.get<GrpcGoogleDefaultTlsOnlyOption>()) {
        result = MakeTlsGoogleDefaultStrategy(cfg, options);
        return;
      }
      result = std::make_unique<GrpcChannelCredentialsAuthentication>(
          grpc::GoogleDefaultCredentials());
    }
    void visit(AccessTokenConfig const& cfg) override {
      result = std::make_unique<GrpcAccessTokenAuthentication>(
          cfg.access_token(), std::move(options));
    }
    void visit(ImpersonateServiceAccountConfig const& cfg) override {
      result = GrpcImpersonateServiceAccount::Create(std::move(cq), cfg,
                                                     std::move(options));
    }
    void visit(ServiceAccountConfig const& cfg) override {
      if (cfg.file_path().has_value()) {
        std::ifstream is(*cfg.file_path());
        if (!is.is_open()) {
          // We use kUnknown here because we don't know if the file does not
          // exist, or if we were unable to open it for some other reason.
          result = std::make_unique<GrpcErrorCredentialsAuthentication>(
              ErrorCredentialsConfig{UnknownError(
                  "Cannot open credentials file " + *cfg.file_path(),
                  GCP_ERROR_INFO())});
        }
        std::string contents(std::istreambuf_iterator<char>{is}, {});
        result = std::make_unique<GrpcServiceAccountAuthentication>(
            std::move(contents), std::move(options));
      } else if (cfg.json_object().has_value()) {
        result = std::make_unique<GrpcServiceAccountAuthentication>(
            *cfg.json_object(), std::move(options));
      } else {
        result = std::make_unique<GrpcErrorCredentialsAuthentication>(
            ErrorCredentialsConfig{InternalError(
                "ServiceAccountConfig has neither json_object nor file_path",
                GCP_ERROR_INFO())});
      }
    }
    void visit(ExternalAccountConfig const& cfg) override {
      auto external_account_creds = GrpcExternalAccountCredentials(cfg);
      if (!external_account_creds) {
        result = std::make_unique<GrpcErrorCredentialsAuthentication>(
            ErrorCredentialsConfig{InvalidArgumentError(
                "Error creating grpc::ExternalAccountCredentials",
                GCP_ERROR_INFO())});
        return;
      }
      grpc::SslCredentialsOptions ssl_options;
      auto cainfo = LoadCAInfo(options);
      if (cainfo) ssl_options.pem_root_certs = std::move(*cainfo);
      result = std::make_unique<GrpcChannelCredentialsAuthentication>(
          grpc::CompositeChannelCredentials(grpc::SslCredentials(ssl_options),
                                            std::move(external_account_creds)));
    }
    void visit(ApiKeyConfig const& cfg) override {
      result = std::make_unique<GrpcApiKeyAuthentication>(cfg.api_key());
    }
    void visit(ComputeEngineCredentialsConfig const&) override {
      result = std::make_unique<GrpcComputeEngineAuthentication>(options);
    }
    void visit(AuthorizedUserConfig const&) override {
      result = std::make_unique<GrpcErrorCredentialsAuthentication>(
          ErrorCredentialsConfig{UnimplementedError(
              "User account credentials (MakeUserAccountCredentials) are not "
              "supported with gRPC transport. Use Google Default Credentials "
              "or Access Token Credentials instead.",
              GCP_ERROR_INFO())});
    }
    void visit(GDCHServiceAccountConfig const& cfg) override {
#if defined(GRPC_CPP_VERSION_MAJOR) && \
    (GRPC_CPP_VERSION_MAJOR > 1 ||     \
     (GRPC_CPP_VERSION_MAJOR == 1 && GRPC_CPP_VERSION_MINOR >= 84))
      std::string json_contents;
      if (cfg.file_path().has_value()) {
        std::ifstream is(*cfg.file_path());
        if (!is.is_open()) {
          result = std::make_unique<GrpcErrorCredentialsAuthentication>(
              ErrorCredentialsConfig{UnknownError(
                  "Cannot open credentials file " + *cfg.file_path(),
                  GCP_ERROR_INFO())});
          return;
        }
        json_contents = std::string{std::istreambuf_iterator<char>{is}, {}};
      } else if (!cfg.json_object().empty()) {
        json_contents = cfg.json_object();
      } else {
        result = std::make_unique<GrpcErrorCredentialsAuthentication>(
            ErrorCredentialsConfig{
                InternalError("GDCHServiceAccountConfig has neither "
                              "json_object nor file_path",
                              GCP_ERROR_INFO())});
        return;
      }

      std::shared_ptr<grpc::CallCredentials> gdch_creds =
          grpc::GDCHServiceAccountCredentials(json_contents, cfg.audience());
      if (!gdch_creds) {
        result = std::make_unique<GrpcErrorCredentialsAuthentication>(
            ErrorCredentialsConfig{InternalError(
                "Error creating grpc::GDCHServiceAccountCredentials",
                GCP_ERROR_INFO())});
        return;
      }

      std::string ca_cert_path;
      nlohmann::json j = nlohmann::json::parse(json_contents, nullptr, false);
      if (!j.is_discarded() && j.is_object()) {
        auto it = j.find("ca_cert_path");
        if (it != j.end() && it->is_string()) {
          ca_cert_path = it->get<std::string>();
        }
      }
      grpc::SslCredentialsOptions ssl_options;
      if (!ca_cert_path.empty()) {
        std::ifstream is(ca_cert_path);
        if (!is.is_open()) {
          result = std::make_unique<GrpcErrorCredentialsAuthentication>(
              ErrorCredentialsConfig{UnknownError(
                  "Cannot open CA certificate file " + ca_cert_path,
                  GCP_ERROR_INFO())});
          return;
        }
        ssl_options.pem_root_certs =
            std::string{std::istreambuf_iterator<char>{is.rdbuf()}, {}};
      } else {
        std::optional<std::string> cainfo = LoadCAInfo(options);
        if (cainfo) ssl_options.pem_root_certs = std::move(*cainfo);
      }
      result = std::make_unique<GrpcChannelCredentialsAuthentication>(
          grpc::CompositeChannelCredentials(grpc::SslCredentials(ssl_options),
                                            gdch_creds));
#else
      (void)cfg;
      result = std::make_unique<GrpcErrorCredentialsAuthentication>(
          ErrorCredentialsConfig{UnimplementedError(
              "GDCHServiceAccountCredentials require gRPC v1.84.0 or greater",
              GCP_ERROR_INFO())});
#endif
    }

  } visitor(std::move(cq), std::move(options));

  CredentialsVisitor::dispatch(credentials, visitor);
  return std::move(visitor.result);
}

std::shared_ptr<GrpcAuthenticationStrategy> CreateAuthenticationStrategy(
    std::shared_ptr<grpc::ChannelCredentials> const& credentials) {
  return std::make_shared<GrpcChannelCredentialsAuthentication>(credentials);
}

std::optional<std::string> LoadCAInfo(Options const& opts) {
  if (!opts.has<CARootsFilePathOption>()) return std::nullopt;
  std::ifstream is(opts.get<CARootsFilePathOption>());
  return std::string{std::istreambuf_iterator<char>{is.rdbuf()}, {}};
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
