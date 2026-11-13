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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_UNIFIED_GRPC_CREDENTIALS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_UNIFIED_GRPC_CREDENTIALS_H

#include "google/cloud/completion_queue.h"
#include "google/cloud/credentials.h"
#include "google/cloud/future.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <grpcpp/grpcpp.h>
#include <memory>
#include <optional>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * Use TLS channel credentials for `MakeGoogleDefaultCredentials()`.
 *
 * `grpc::GoogleDefaultCredentials()` selects ALTS for the endpoints of xDS
 * clusters that are not fronted by Google's edge proxies. Some such clusters,
 * for example DirectPath over Interconnect, serve hosts outside Google Cloud,
 * where ALTS is unavailable. With this option the authentication strategy for
 * Google Default credentials composes `grpc::SslCredentials()` with the call
 * credentials that Application Default Credentials resolve to, attached at the
 * channel level so that gRPC's control-plane channels (such as RLS) inherit
 * them.
 *
 * Libraries set this option for endpoints known to require TLS; it is not
 * intended for applications.
 */
struct GrpcGoogleDefaultTlsOnlyOption {
  using Type = bool;
};

/**
 * The outcome of the Application Default Credentials file lookup, classified
 * the way gRPC's Google Default credentials classify it.
 */
enum class AdcSource {
  kNone,  ///< No file found; use Compute Engine credentials.
  kServiceAccount,
  kAuthorizedUser,
  kExternalAccount,
  kNotJsonObject,    ///< The file does not contain a JSON object.
  kUnsupportedType,  ///< A `type` gRPC's Google Default credentials reject.
};

/// The parsed result of `LookupApplicationDefaultCredentials()`.
struct AdcLookup {
  AdcSource source = AdcSource::kNone;
  std::optional<std::string> path;      ///< Set unless `source` is `kNone`.
  std::optional<std::string> contents;  ///< Set unless `source` is `kNone`.
  std::optional<std::string> type;      ///< The JSON `type`, if it is a string.
};

/**
 * Finds and reads the Application Default Credentials file.
 *
 * Uses the same search order as the REST credentials:
 * `GOOGLE_APPLICATION_CREDENTIALS`, then the gcloud file (the
 * `GOOGLE_GCLOUD_ADC_PATH_OVERRIDE` path or the well-known location), which
 * is used only if it exists. gRPC's Google Default credentials search the same
 * locations, minus the override. Returns an error only when a configured file
 * cannot be opened; every other outcome is described by `AdcLookup::source`.
 */
StatusOr<AdcLookup> LookupApplicationDefaultCredentials();

class GrpcAuthenticationStrategy {
 public:
  virtual ~GrpcAuthenticationStrategy() = default;

  virtual std::shared_ptr<grpc::Channel> CreateChannel(
      std::string const& endpoint, grpc::ChannelArguments const& arguments) = 0;
  virtual bool RequiresConfigureContext() const = 0;
  virtual Status ConfigureContext(grpc::ClientContext& context) = 0;
  virtual future<StatusOr<std::shared_ptr<grpc::ClientContext>>>
  AsyncConfigureContext(std::shared_ptr<grpc::ClientContext> context) = 0;
};

std::shared_ptr<GrpcAuthenticationStrategy> CreateAuthenticationStrategy(
    google::cloud::CompletionQueue cq, Options const& options);

std::shared_ptr<GrpcAuthenticationStrategy> CreateAuthenticationStrategy(
    Credentials const& credentials, CompletionQueue cq, Options options = {});

std::shared_ptr<GrpcAuthenticationStrategy> CreateAuthenticationStrategy(
    std::shared_ptr<grpc::ChannelCredentials> const& credentials);

std::optional<std::string> LoadCAInfo(Options const& opts);

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_UNIFIED_GRPC_CREDENTIALS_H
