// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_CLIENT_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_CLIENT_OPTIONS_H

#include "google/cloud/storage/oauth2/credentials.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/common_options.h"
#include "google/cloud/options.h"
#include <chrono>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
class ClientOptions;
namespace internal {
std::string JsonEndpoint(Options const&);
std::string JsonUploadEndpoint(Options const&);
std::string XmlEndpoint(Options const&);
std::string IamEndpoint(Options const&);

Options MakeOptions(ClientOptions);

ClientOptions MakeBackwardsCompatibleClientOptions(Options);

Options ApplyPolicy(Options opts, RetryPolicy const& p);
Options ApplyPolicy(Options opts, BackoffPolicy const& p);
Options ApplyPolicy(Options opts, IdempotencyPolicy const& p);

inline Options ApplyPolicies(Options opts) { return opts; }

template <typename P, typename... Policies>
Options ApplyPolicies(Options opts, P&& head, Policies&&... tail) {
  opts = ApplyPolicy(std::move(opts), std::forward<P>(head));
  return ApplyPolicies(std::move(opts), std::forward<Policies>(tail)...);
}

Options DefaultOptions(std::shared_ptr<oauth2::Credentials> credentials,
                       Options opts);
Options DefaultOptionsWithCredentials(Options opts);

}  // namespace internal

/**
 * Describes the configuration for low-level connection features.
 *
 * Some applications may want to use a different SSL root of trust for their
 * connections, for example, containerized applications might store the
 * certificate authority certificates in a hard-coded location.
 *
 * This is a separate class, as it is used to configure both the normal
 * connections to GCS and the connections used to obtain OAuth2 access
 * tokens.
 */
class ChannelOptions {
 public:
  /// @deprecated Use google::cloud::Options and CAPathOption instead.
  std::string ssl_root_path() const { return ssl_root_path_; }

  /// @deprecated Use google::cloud::Options and CAPathOption instead.
  ChannelOptions& set_ssl_root_path(std::string ssl_root_path) {
    ssl_root_path_ = std::move(ssl_root_path);
    return *this;
  }

 private:
  std::string ssl_root_path_;
};

/**
 * Describes the configuration for a `storage::Client` object.
 *
 * By default, several environment variables are read to configure the client:
 *
 * - `CLOUD_STORAGE_EMULATOR_ENDPOINT`: if set, use this http endpoint to
 *   make all http requests instead of the production GCS service. Also,
 *   if set, the `CreateDefaultClientOptions()` function will use an
 *   `AnonymousCredentials` object instead of loading Application Default
 *   %Credentials.
 * - `CLOUD_STORAGE_ENABLE_CLOG`: if set, enable std::clog as a backend for
 *   `google::cloud::LogSink`.
 * - `CLOUD_STORAGE_ENABLE_TRACING`: if set, this is the list of components that
 *   will have logging enabled, the component this is:
 *   - `http`: trace all http request / responses.
 *
 * @deprecated Please use google::cloud::Options instead.
 */
class ClientOptions {
 public:
  /**
   * Constructor with channel options.
   *
   * @deprecated use google::cloud::Options instead.
   *
   * @param credentials how to authenticate to the client. Using a `nullptr` for
   *   @p credentials results in undefined behavior.
   */
  explicit ClientOptions(std::shared_ptr<oauth2::Credentials> credentials)
      : ClientOptions(std::move(credentials), {}) {}

  /**
   * Constructor with channel options.
   *
   * @deprecated use google::cloud::Options instead.
   *
   * @param credentials how to authenticate to the client. Using a `nullptr` for
   *   @p credentials results in undefined behavior.
   * @param channel_options the configuration for SSL certificate validation.
   */
  ClientOptions(std::shared_ptr<oauth2::Credentials> credentials,
                ChannelOptions channel_options);

  /**
   * Creates a `ClientOptions` with Google Application Default %Credentials.
   *
   * If Application Default %Credentials could not be loaded, this returns a
   * `Status` with failure details.  If the `CLOUD_STORAGE_EMULATOR_ENDPOINT`
   * environment variable is set, this function instead uses an
   * `AnonymousCredentials` to configure the client.
   *
   * @deprecated Please use google::cloud::Options instead.
   */
  static StatusOr<ClientOptions> CreateDefaultClientOptions();
  /// @deprecated Please use google::cloud::Options instead.
  static StatusOr<ClientOptions> CreateDefaultClientOptions(
      ChannelOptions const& channel_options);

  /**
   * @deprecated Use google::cloud::Options and Oauth2CredentialsOption instead.
   */
  std::shared_ptr<oauth2::Credentials> credentials() const {
    return opts_.get<Oauth2CredentialsOption>();
  }

  /**
   * @deprecated Use google::cloud::Options and Oauth2CredentialsOption instead.
   */
  ClientOptions& set_credentials(std::shared_ptr<oauth2::Credentials> c) {
    opts_.set<Oauth2CredentialsOption>(std::move(c));
    return *this;
  }

  /**
   * @deprecated Use google::cloud::Options and RestEndpointOption instead.
   */
  std::string const& endpoint() const {
    return opts_.get<RestEndpointOption>();
  }

  /**
   * @deprecated Use google::cloud::Options and RestEndpointOption instead.
   */
  ClientOptions& set_endpoint(std::string endpoint) {
    opts_.set<RestEndpointOption>(std::move(endpoint));
    return *this;
  }

  /**
   * @deprecated Use google::cloud::Options and IamEndpointOption instead.
   */
  std::string const& iam_endpoint() const {
    return opts_.get<IamEndpointOption>();
  }

  /**
   * @deprecated Use google::cloud::Options and IamEndpointOption instead.
   */
  ClientOptions& set_iam_endpoint(std::string endpoint) {
    opts_.set<IamEndpointOption>(std::move(endpoint));
    return *this;
  }

  /**
   * @deprecated This was intended for development and not a public API.
   */
  std::string const& version() const {
    return opts_.get<internal::TargetApiVersionOption>();
  }

  /**
   * @deprecated This was intended for development and not a public API.
   */
  ClientOptions& set_version(std::string version) {
    opts_.set<internal::TargetApiVersionOption>(std::move(version));
    return *this;
  }

  /**
   * @deprecated Use google::cloud::Options and
   *     google::cloud::TracingComponentsOption instead.
   */
  bool enable_http_tracing() const;

  /**
   * @deprecated Use google::cloud::Options and
   *     google::cloud::TracingComponentsOption instead.
   */
  ClientOptions& set_enable_http_tracing(bool enable);

  /**
   * @deprecated Use google::cloud::Options and
   *     google::cloud::TracingComponentsOption instead.
   */
  bool enable_raw_client_tracing() const;

  /**
   * @deprecated Use google::cloud::Options and
   *     google::cloud::TracingComponentsOption instead.
   */
  ClientOptions& set_enable_raw_client_tracing(bool enable);

  /**
   * @deprecated Use google::cloud::Options and ProjectIdOption instead.
   */
  std::string const& project_id() const { return opts_.get<ProjectIdOption>(); }

  /**
   * @deprecated Use google::cloud::Options and ProjectIdOption instead.
   */
  ClientOptions& set_project_id(std::string v) {
    opts_.set<ProjectIdOption>(std::move(v));
    return *this;
  }

  /**
   * @deprecated Use google::cloud::Options and ConnectionPoolSizeOption
   *     instead.
   */
  std::size_t connection_pool_size() const {
    return opts_.get<ConnectionPoolSizeOption>();
  }

  /**
   * @deprecated Use google::cloud::Options and ConnectionPoolSizeOption
   *     instead.
   */
  ClientOptions& set_connection_pool_size(std::size_t size) {
    opts_.set<ConnectionPoolSizeOption>(size);
    return *this;
  }

  /**
   * @deprecated Use google::cloud::Options and DownloadBufferSizeOption
   *     instead.
   */
  std::size_t download_buffer_size() const {
    return opts_.get<DownloadBufferSizeOption>();
  }

  /**
   * @deprecated Use google::cloud::Options and DownloadBufferSizeOption
   *     instead.
   */
  ClientOptions& SetDownloadBufferSize(std::size_t size);

  /**
   * @deprecated Use google::cloud::Options and UploadBufferSizeOption instead.
   */
  std::size_t upload_buffer_size() const {
    return opts_.get<UploadBufferSizeOption>();
  }

  /**
   * @deprecated Use google::cloud::Options and UploadBufferSizeOption instead.
   */
  ClientOptions& SetUploadBufferSize(std::size_t size);

  /**
   * @deprecated Use google::cloud::Options and
   *     google::cloud::UserAgentProductsOption instead.
   */
  std::string const& user_agent_prefix() const { return user_agent_prefix_; }

  /**
   * @deprecated Use google::cloud::Options and
   *     google::cloud::UserAgentProductsOption instead.
   */
  ClientOptions& add_user_agent_prefix(std::string prefix) {
    opts_.lookup<UserAgentProductsOption>().push_back(prefix);
    if (!user_agent_prefix_.empty()) {
      prefix += ' ';
      prefix += user_agent_prefix_;
    }
    user_agent_prefix_ = std::move(prefix);
    return *this;
  }

  /// @deprecated use `add_user_agent_prefix()` instead.
  ClientOptions& add_user_agent_prefx(std::string const& v) {
    return add_user_agent_prefix(v);
  }

  /**
   * @deprecated Use google::cloud::Options and MaximumSimpleUploadSizeOption
   *     instead.
   */
  std::size_t maximum_simple_upload_size() const {
    return opts_.get<MaximumSimpleUploadSizeOption>();
  }

  /**
   * @deprecated Use google::cloud::Options and MaximumSimpleUploadSizeOption
   *     instead.
   */
  ClientOptions& set_maximum_simple_upload_size(std::size_t v) {
    opts_.set<MaximumSimpleUploadSizeOption>(v);
    return *this;
  }

  /**
   * If true and using OpenSSL 1.0.2 the library configures the OpenSSL
   * callbacks for locking.
   *
   * @deprecated Use google::cloud::options and EnableCurlSslLockingOption
   *     instead.
   */
  bool enable_ssl_locking_callbacks() const {
    return opts_.get<EnableCurlSslLockingOption>();
  }

  /**
   * If true and using OpenSSL 1.0.2 the library configures the OpenSSL
   * callbacks for locking.
   *
   * @deprecated Use google::cloud::options and EnableCurlSslLockingOption
   *     instead.
   */
  ClientOptions& set_enable_ssl_locking_callbacks(bool v) {
    opts_.set<EnableCurlSslLockingOption>(v);
    return *this;
  }

  /**
   * @deprecated Use google::cloud::Options and EnableCurlSigpipeOption
   *     instead.
   */
  bool enable_sigpipe_handler() const {
    return opts_.get<EnableCurlSigpipeHandlerOption>();
  }

  /**
   * @deprecated Use google::cloud::Options and EnableCurlSigpipeOption
   *     instead.
   */
  ClientOptions& set_enable_sigpipe_handler(bool v) {
    opts_.set<EnableCurlSigpipeHandlerOption>(v);
    return *this;
  }

  /**
   * @deprecated Use google::cloud::Options and MaximumCurlSocketRecvSizeOption
   *     instead.
   */
  std::size_t maximum_socket_recv_size() const {
    return opts_.get<MaximumCurlSocketRecvSizeOption>();
  }

  /**
   * @deprecated Use google::cloud::Options and MaximumCurlSocketRecvSizeOption
   *     instead.
   */
  ClientOptions& set_maximum_socket_recv_size(std::size_t v) {
    opts_.set<MaximumCurlSocketRecvSizeOption>(v);
    return *this;
  }

  /**
   * @deprecated Use google::cloud::Options and MaximumCurlSocketSendSizeOption
   *     instead.
   */
  std::size_t maximum_socket_send_size() const {
    return opts_.get<MaximumCurlSocketSendSizeOption>();
  }

  /**
   * @deprecated Use google::cloud::Options and MaximumCurlSocketSendSizeOption
   *     instead.
   */
  ClientOptions& set_maximum_socket_send_size(std::size_t v) {
    opts_.set<MaximumCurlSocketSendSizeOption>(v);
    return *this;
  }

  /// @deprecated Use google::cloud::Options and CAPathOption instead.
  ChannelOptions& channel_options() { return channel_options_; }

  /// @deprecated Use google::cloud::Options and CAPathOption instead.
  ChannelOptions const& channel_options() const { return channel_options_; }

  //@{
  /**
   * Control the maximum amount of time allowed for "stalls" during a download.
   *
   * A download that receives no data is considered "stalled". If the download
   * remains stalled for more than the time set in this option then the download
   * is aborted.
   *
   * The default value is 2 minutes. Can be disabled by setting the value to 0.
   *
   * @deprecated Use google::cloud::Options and TransferStallTimeoutOption
   *     instead.
   */
  std::chrono::seconds download_stall_timeout() const {
    return opts_.get<TransferStallTimeoutOption>();
  }

  /**
   * @deprecated Use google::cloud::Options and TransferStallTimeoutOption
   *     instead.
   */
  ClientOptions& set_download_stall_timeout(std::chrono::seconds v) {
    opts_.set<TransferStallTimeoutOption>(std::move(v));
    return *this;
  }
  //@}

 private:
  friend Options internal::MakeOptions(ClientOptions);
  friend ClientOptions internal::MakeBackwardsCompatibleClientOptions(Options);

  explicit ClientOptions(Options o);

  Options opts_;

  // Used for backwards compatibility. The value here is merged with the values
  // in `opts_` by internal::MakeOptions(ClientOptions const&);
  ChannelOptions channel_options_;

  // Only used for backwards compatibility, the value in `opts_.
  std::string user_agent_prefix_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_CLIENT_OPTIONS_H
