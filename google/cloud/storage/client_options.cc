// Copyright 2018 Google LLC
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

#include "google/cloud/storage/client_options.h"
#include "google/cloud/storage/internal/unified_rest_credentials.h"
#include "google/cloud/storage/oauth2/credentials.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/curl_options.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/rest_options.h"
#include "google/cloud/internal/rest_response.h"
#include "google/cloud/log.h"
#include "absl/strings/str_split.h"
#include <cstdlib>
#include <set>
#include <sstream>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::internal::GetEnv;

namespace {

absl::optional<std::string> GetEmulator() {
  auto emulator = GetEnv("CLOUD_STORAGE_EMULATOR_ENDPOINT");
  if (emulator) return emulator;
  return GetEnv("CLOUD_STORAGE_TESTBENCH_ENDPOINT");
}

StatusOr<std::shared_ptr<oauth2::Credentials>> StorageDefaultCredentials(
    ChannelOptions const& channel_options) {
  auto emulator = GetEmulator();
  if (emulator.has_value()) {
    return StatusOr<std::shared_ptr<oauth2::Credentials>>(
        oauth2::CreateAnonymousCredentials());
  }
  return oauth2::GoogleDefaultCredentials(channel_options);
}

std::size_t DefaultConnectionPoolSize() {
  std::size_t nthreads = std::thread::hardware_concurrency();
  if (nthreads == 0) {
    return 4;
  }
  return 4 * nthreads;
}

// This magic number was obtained by experimentation summarized in #2657
#ifndef GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_UPLOAD_BUFFER_SIZE
#define GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_UPLOAD_BUFFER_SIZE (8 * 1024 * 1024)
#endif  // GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_BUFFER_SIZE

// This magic number was obtained by experimentation summarized in #2657
#ifndef GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_DOWNLOAD_BUFFER_SIZE
#define GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_DOWNLOAD_BUFFER_SIZE \
  (3 * 1024 * 1024 / 2)
#endif  // GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_BUFFER_SIZE

// This is a result of experiments performed in #2657.
#ifndef GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_MAXIMUM_SIMPLE_UPLOAD_SIZE
#define GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_MAXIMUM_SIMPLE_UPLOAD_SIZE \
  (20 * 1024 * 1024L)
#endif  // GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_MAXIMUM_SIMPLE_UPLOAD_SIZE

#ifndef GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_DOWNLOAD_STALL_TIMEOUT
#define GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_DOWNLOAD_STALL_TIMEOUT 120
#endif  // GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_DOWNLOAD_STALL_TIMEOUT

// Define the defaults using a pre-processor macro, this allows the application
// developers to change the defaults for their application by compiling with
// different values.
#ifndef STORAGE_CLIENT_DEFAULT_MAXIMUM_RETRY_PERIOD
#define STORAGE_CLIENT_DEFAULT_MAXIMUM_RETRY_PERIOD std::chrono::minutes(15)
#endif  // STORAGE_CLIENT_DEFAULT_MAXIMUM_RETRY_PERIOD

#ifndef STORAGE_CLIENT_DEFAULT_INITIAL_BACKOFF_DELAY
#define STORAGE_CLIENT_DEFAULT_INITIAL_BACKOFF_DELAY std::chrono::seconds(1)
#endif  // STORAGE_CLIENT_DEFAULT_INITIAL_BACKOFF_DELAY

#ifndef STORAGE_CLIENT_DEFAULT_MAXIMUM_BACKOFF_DELAY
#define STORAGE_CLIENT_DEFAULT_MAXIMUM_BACKOFF_DELAY std::chrono::minutes(5)
#endif  // STORAGE_CLIENT_DEFAULT_MAXIMUM_BACKOFF_DELAY

#ifndef STORAGE_CLIENT_DEFAULT_BACKOFF_SCALING
#define STORAGE_CLIENT_DEFAULT_BACKOFF_SCALING 2.0
#endif  //  STORAGE_CLIENT_DEFAULT_BACKOFF_SCALING

}  // namespace

namespace internal {

std::string RestEndpoint(Options const& options) {
  return GetEmulator().value_or(options.get<RestEndpointOption>());
}

std::string IamRestEndpoint(Options const& options) {
  return GetEmulator().value_or(options.get<IamEndpointOption>());
}

std::string IamRestPath() {
  auto emulator = GetEmulator();
  if (emulator) return "/iamapi";
  return {};
}

std::string JsonEndpoint(Options const& options) {
  return GetEmulator().value_or(options.get<RestEndpointOption>()) +
         "/storage/" + options.get<TargetApiVersionOption>();
}

std::string JsonUploadEndpoint(Options const& options) {
  return GetEmulator().value_or(options.get<RestEndpointOption>()) +
         "/upload/storage/" + options.get<TargetApiVersionOption>();
}

std::string XmlEndpoint(Options const& options) {
  return GetEmulator().value_or(options.get<RestEndpointOption>());
}

std::string IamEndpoint(Options const& options) {
  auto emulator = GetEmulator();
  if (emulator) return *emulator + "/iamapi";
  return options.get<IamEndpointOption>();
}

Options MakeOptions(ClientOptions o) {
  auto opts = std::move(o.opts_);
  if (!o.channel_options().ssl_root_path().empty()) {
    opts.set<CARootsFilePathOption>(o.channel_options().ssl_root_path());
  }
  return opts;
}

ClientOptions MakeBackwardsCompatibleClientOptions(Options opts) {
  return ClientOptions(std::move(opts));
}

Options ApplyPolicy(Options opts, RetryPolicy const& p) {
  opts.set<RetryPolicyOption>(p.clone());
  return opts;
}

Options ApplyPolicy(Options opts, BackoffPolicy const& p) {
  opts.set<BackoffPolicyOption>(p.clone());
  return opts;
}

Options ApplyPolicy(Options opts, IdempotencyPolicy const& p) {
  opts.set<IdempotencyPolicyOption>(p.clone());
  return opts;
}

Options DefaultOptions(std::shared_ptr<oauth2::Credentials> credentials,
                       Options opts) {
  auto o =
      Options{}
          .set<Oauth2CredentialsOption>(std::move(credentials))
          .set<RestEndpointOption>("https://storage.googleapis.com")
          .set<IamEndpointOption>("https://iamcredentials.googleapis.com/v1")
          .set<TargetApiVersionOption>("v1")
          .set<ConnectionPoolSizeOption>(DefaultConnectionPoolSize())
          .set<DownloadBufferSizeOption>(
              GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_DOWNLOAD_BUFFER_SIZE)
          .set<UploadBufferSizeOption>(
              GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_UPLOAD_BUFFER_SIZE)
          .set<MaximumSimpleUploadSizeOption>(
              GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_MAXIMUM_SIMPLE_UPLOAD_SIZE)
          .set<EnableCurlSslLockingOption>(true)
          .set<EnableCurlSigpipeHandlerOption>(true)
          .set<MaximumCurlSocketRecvSizeOption>(0)
          .set<MaximumCurlSocketSendSizeOption>(0)
          .set<TransferStallTimeoutOption>(std::chrono::seconds(
              GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_DOWNLOAD_STALL_TIMEOUT))
          .set<RetryPolicyOption>(
              LimitedTimeRetryPolicy(
                  STORAGE_CLIENT_DEFAULT_MAXIMUM_RETRY_PERIOD)
                  .clone())
          .set<BackoffPolicyOption>(
              ExponentialBackoffPolicy(
                  STORAGE_CLIENT_DEFAULT_INITIAL_BACKOFF_DELAY,
                  STORAGE_CLIENT_DEFAULT_MAXIMUM_BACKOFF_DELAY,
                  STORAGE_CLIENT_DEFAULT_BACKOFF_SCALING)
                  .clone())
          .set<IdempotencyPolicyOption>(AlwaysRetryIdempotencyPolicy().clone());

  o = google::cloud::internal::MergeOptions(std::move(opts), std::move(o));
  // If the application did not set `DownloadStallTimeoutOption` then use the
  // same value as `TransferStallTimeoutOption` (which could be the default
  // value). Some applications need tighter timeouts for downloads, but longer
  // timeouts for other transfers.
  if (!o.has<DownloadStallTimeoutOption>()) {
    o.set<DownloadStallTimeoutOption>(o.get<TransferStallTimeoutOption>());
  }

  auto emulator = GetEmulator();
  if (emulator.has_value()) {
    o.set<RestEndpointOption>(*emulator).set<IamEndpointOption>(*emulator +
                                                                "/iamapi");
  }

  auto tracing = GetEnv("CLOUD_STORAGE_ENABLE_TRACING");
  if (tracing.has_value()) {
    for (auto c : absl::StrSplit(*tracing, ',')) {
      GCP_LOG(INFO) << "Enabling logging for " << c;
      o.lookup<TracingComponentsOption>().insert(std::string(c));
    }
  }

  auto project_id = GetEnv("GOOGLE_CLOUD_PROJECT");
  if (project_id.has_value()) {
    o.set<ProjectIdOption>(std::move(*project_id));
  }

  auto use_rest = GetEnv("GOOGLE_CLOUD_CPP_STORAGE_HAVE_REST_CLIENT");
  if (use_rest.has_value()) o.set<UseRestClientOption>(true);

  // Always apply the RestClient defaults, even if it is not in use. Now that we
  // use the low-level initialization code in
  // google/cloud/internal/curl_wrappers.cc, these are always needed.
  namespace rest = ::google::cloud::rest_internal;
  auto rest_defaults =
      Options{}
          .set<rest_internal::DownloadStallTimeoutOption>(
              o.get<DownloadStallTimeoutOption>())
          .set<rest::TransferStallTimeoutOption>(
              o.get<TransferStallTimeoutOption>())
          .set<rest::MaximumCurlSocketRecvSizeOption>(
              o.get<MaximumCurlSocketRecvSizeOption>())
          .set<rest::MaximumCurlSocketSendSizeOption>(
              o.get<MaximumCurlSocketSendSizeOption>())
          .set<rest::HttpVersionOption>(
              o.get<storage_experimental::HttpVersionOption>())
          .set<rest::CAPathOption>(o.get<internal::CAPathOption>())
          .set<rest::ConnectionPoolSizeOption>(
              o.get<ConnectionPoolSizeOption>())
          .set<rest::EnableCurlSslLockingOption>(
              o.get<EnableCurlSslLockingOption>())
          .set<rest::EnableCurlSigpipeHandlerOption>(
              o.get<EnableCurlSigpipeHandlerOption>())
          // This prevents the RestClient from treating these codes as errors.
          // Instead, it will  allow them to propagate back to the calling code
          // where it can determine if they are indeed errors or not.
          .set<rest::IgnoredHttpErrorCodes>(
              {rest::HttpStatusCode::kResumeIncomplete,
               rest::HttpStatusCode::kClientClosedRequest});

  return google::cloud::internal::MergeOptions(std::move(o),
                                               std::move(rest_defaults));
}

Options DefaultOptionsWithCredentials(Options opts) {
  if (opts.has<Oauth2CredentialsOption>()) {
    auto credentials = opts.get<Oauth2CredentialsOption>();
    return internal::DefaultOptions(std::move(credentials), std::move(opts));
  }
  if (opts.has<UnifiedCredentialsOption>()) {
    auto credentials =
        internal::MapCredentials(opts.get<UnifiedCredentialsOption>());
    return internal::DefaultOptions(std::move(credentials), std::move(opts));
  }
  if (GetEmulator().has_value()) {
    return internal::DefaultOptions(
        internal::MapCredentials(google::cloud::MakeInsecureCredentials()),
        std::move(opts));
  }
  return internal::DefaultOptions(
      internal::MapCredentials(google::cloud::MakeGoogleDefaultCredentials()),
      std::move(opts));
}

}  // namespace internal

StatusOr<ClientOptions> ClientOptions::CreateDefaultClientOptions() {
  return CreateDefaultClientOptions(ChannelOptions{});
}

StatusOr<ClientOptions> ClientOptions::CreateDefaultClientOptions(
    ChannelOptions const& channel_options) {
  auto creds = StorageDefaultCredentials(channel_options);
  if (!creds) return creds.status();
  return ClientOptions(*creds, channel_options);
}

ClientOptions::ClientOptions(std::shared_ptr<oauth2::Credentials> credentials,
                             ChannelOptions channel_options)
    : opts_(internal::DefaultOptions(std::move(credentials), {})),
      channel_options_(std::move(channel_options)) {}

ClientOptions::ClientOptions(Options o)
    : opts_(std::move(o)),
      user_agent_prefix_(
          absl::StrJoin(opts_.get<UserAgentProductsOption>(), " ")) {
  channel_options_.set_ssl_root_path(opts_.get<CARootsFilePathOption>());
}

bool ClientOptions::enable_http_tracing() const {
  return opts_.get<TracingComponentsOption>().count("http") != 0;
}

ClientOptions& ClientOptions::set_enable_http_tracing(bool enable) {
  if (enable) {
    opts_.lookup<TracingComponentsOption>().insert("http");
  } else {
    opts_.lookup<TracingComponentsOption>().erase("http");
  }
  return *this;
}

bool ClientOptions::enable_raw_client_tracing() const {
  return opts_.get<TracingComponentsOption>().count("raw-client") != 0;
}

ClientOptions& ClientOptions::set_enable_raw_client_tracing(bool enable) {
  if (enable) {
    opts_.lookup<TracingComponentsOption>().insert("raw-client");
  } else {
    opts_.lookup<TracingComponentsOption>().erase("raw-client");
  }
  return *this;
}

ClientOptions& ClientOptions::SetDownloadBufferSize(std::size_t size) {
  opts_.set<DownloadBufferSizeOption>(
      size == 0 ? GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_DOWNLOAD_BUFFER_SIZE : size);
  return *this;
}

ClientOptions& ClientOptions::SetUploadBufferSize(std::size_t size) {
  opts_.set<UploadBufferSizeOption>(
      size == 0 ? GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_UPLOAD_BUFFER_SIZE : size);
  return *this;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
