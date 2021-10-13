// Copyright 2017 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_CLIENT_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_CLIENT_OPTIONS_H

#include "google/cloud/bigtable/options.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/background_threads.h"
#include "google/cloud/common_options.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/options.h"
#include "google/cloud/status.h"
#include "google/cloud/tracing_options.h"
#include "absl/strings/str_split.h"
#include <grpcpp/grpcpp.h>
#include <grpcpp/resource_quota.h>
#include <chrono>
#include <set>
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
class ClientOptions;
namespace internal {
Options&& MakeOptions(ClientOptions&& o);
}  // namespace internal

/**
 * Configuration options for the Bigtable Client.
 *
 * Applications typically configure the client class using:
 * @code
 * auto client =
 *   bigtable::Client(bigtable::ClientOptions().SetCredentials(...));
 * @endcode
 */
class ClientOptions {
 public:
  /**
   * Initialize the client options.
   *
   * Configure the client to connect to the Cloud Bigtable service, using the
   * default options.
   *
   * @par Environment Variables
   * If the `BIGTABLE_EMULATOR_HOST` environment variable is set, the default
   * configuration changes in important ways:
   *
   * - The credentials are initialized to `grpc::InsecureCredentials()`.
   * - Any client created with these objects will connect to the endpoint
   *   (typically just a `host:port` string) set in the environment variable.
   *
   * This makes it easy to test applications using the Cloud Bigtable Emulator.
   *
   * @see The Google Cloud Platform introduction to
   * [application default
   * credentials](https://cloud.google.com/docs/authentication/production)
   * @see `grpc::GoogleDefaultCredentials` in the [grpc
   * documentation](https://grpc.io/grpc/cpp/namespacegrpc.html)
   * @see The [documentation](https://cloud.google.com/bigtable/docs/emulator)
   * for the Cloud Bigtable Emulator.
   */
  ClientOptions();

  /**
   * Initialize the client options.
   *
   * Expected options are any of the types in the following option lists.
   *
   * - `google::cloud::CommonOptionList`
   * - `google::cloud::GrpcOptionList`
   * - `google::cloud::bigtable::ClientOptionList`
   *
   * @note Unrecognized options will be ignored. To debug issues with options
   *     set `GOOGLE_CLOUD_CPP_ENABLE_CLOG=yes` in the environment and
   *     unexpected options will be logged.
   *
   * @param opts (optional) configuration options
   */
  explicit ClientOptions(Options opts);

  /**
   * Connect to the production instance of Cloud Bigtable using @p creds.
   *
   * This constructor always connects to the production instance of Cloud
   * Bigtable, and can be used when the application default credentials are
   * not configured in the environment where the application is running.
   *
   * @note Prefer using the `ClientOptions(Options opts)` constructor and
   *     passing in @p creds as a `GrpcCredentialOption`.
   *
   * @param creds gRPC authentication credentials
   */
  explicit ClientOptions(std::shared_ptr<grpc::ChannelCredentials> creds);

  /// Return the current endpoint for data RPCs.
  std::string const& data_endpoint() const {
    return opts_.get<DataEndpointOption>();
  }

  ClientOptions& set_data_endpoint(std::string endpoint) {
    opts_.set<DataEndpointOption>(std::move(endpoint));
    return *this;
  }

  /// Return the current endpoint for admin RPCs.
  std::string const& admin_endpoint() const {
    return opts_.get<AdminEndpointOption>();
  }

  ClientOptions& set_admin_endpoint(std::string endpoint) {
    opts_.set<AdminEndpointOption>(endpoint);
    // These two endpoints are generally equivalent, but they may differ in
    // some tests.
    opts_.set<InstanceAdminEndpointOption>(std::move(endpoint));
    return *this;
  }

  /**
   * Set the name of the connection pool.
   *
   * gRPC typically opens a single connection for each destination.  To improve
   * performance, the Cloud Bigtable C++ client can open multiple connections
   * to a given destination, but these connections are shared by all threads
   * in the application.  Sometimes the application may want even more
   * segregation, for example, the application may want to use a different pool
   * for high-priority requests vs. lower priority ones.  Using different names
   * creates segregated pools.
   */
  ClientOptions& set_connection_pool_name(std::string name) {
    connection_pool_name_ = std::move(name);
    return *this;
  }

  /// Return the name of the connection pool.
  std::string const& connection_pool_name() const {
    return connection_pool_name_;
  }

  /**
   * Set the size of the connection pool.
   *
   * Specifying 0 for @p size will set the size of the connection pool to
   * default.
   */
  ClientOptions& set_connection_pool_size(std::size_t size);

  std::size_t connection_pool_size() const {
    return opts_.get<GrpcNumChannelsOption>();
  }

  /// Return the current credentials.
  std::shared_ptr<grpc::ChannelCredentials> credentials() const {
    return opts_.get<GrpcCredentialOption>();
  }

  ClientOptions& SetCredentials(
      std::shared_ptr<grpc::ChannelCredentials> credentials) {
    opts_.set<GrpcCredentialOption>(std::move(credentials));
    return *this;
  }

  /// Access all the channel arguments.
  grpc::ChannelArguments channel_arguments() const {
    return ::google::cloud::internal::MakeChannelArguments(opts_);
  }

  /// Set all the channel arguments.
  ClientOptions& set_channel_arguments(
      grpc::ChannelArguments const& channel_arguments) {
    opts_.set<GrpcChannelArgumentsNativeOption>(channel_arguments);
    return *this;
  }

  /**
   * Set compression algorithm for channel.
   *
   * Please see the docs for grpc::ChannelArguments::SetCompressionAlgorithm()
   * on https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html
   * for more details.
   *
   */
  void SetCompressionAlgorithm(grpc_compression_algorithm algorithm) {
    opts_.lookup<GrpcChannelArgumentsNativeOption>().SetCompressionAlgorithm(
        algorithm);
  }

  /**
   * Set the `grpclb` fallback timeout with the timestamp @p fallback_timeout
   * for the channel.
   *
   * For example:
   *
   * @code
   * bigtable::ClientOptions::SetGrpclbFallbackTimeout(
   *     std::chrono::milliseconds(5000))
   * bigtable::ClientOptions::SetGrpclbFallbackTimeout(
   *     std::chrono::seconds(5))
   * @endcode
   *
   * @tparam Rep a placeholder to match the Rep tparam for @p fallback_timeout,
   *     the semantics of this template parameter are documented in
   *     `std::chrono::duration<>` (in brief, the underlying arithmetic type
   *     used to store the number of ticks), for our purposes it is simply a
   *     formal parameter.
   * @tparam Period a placeholder to match the Period tparam for @p
   *     fallback_timeout, the semantics of this template parameter are
   *     documented in `std::chrono::duration<>` (in brief, the length of the
   *     tick in seconds, expressed as a `std::ratio<>`), for our purposes it
   *     is simply a formal parameter.
   *
   * @see
   * [std::chrono::duration<>](http://en.cppreference.com/w/cpp/chrono/duration)
   *     for more details.
   *
   * Please see the docs for
   * `grpc::ChannelArguments::SetGrpclbFallbackTimeout()` on
   * https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html for more
   * details.
   *
   */
  template <typename Rep, typename Period>
  google::cloud::Status SetGrpclbFallbackTimeout(
      std::chrono::duration<Rep, Period> fallback_timeout) {
    std::chrono::milliseconds ft_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(fallback_timeout);

    if (ft_ms.count() > std::numeric_limits<int>::max()) {
      return google::cloud::Status(google::cloud::StatusCode::kOutOfRange,
                                   "The supplied duration is larger than the "
                                   "maximum value allowed by gRPC (INT_MAX)");
    }
    auto fallback_timeout_ms = static_cast<int>(ft_ms.count());
    opts_.lookup<GrpcChannelArgumentsNativeOption>().SetGrpclbFallbackTimeout(
        fallback_timeout_ms);
    return google::cloud::Status();
  }

  /**
   * Set the string to prepend to the user agent.
   *
   * Please see the docs for `grpc::ChannelArguments::SetUserAgentPrefix()`
   * on https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html
   * for more details.
   *
   */
  void SetUserAgentPrefix(grpc::string const& user_agent_prefix) {
    opts_.lookup<GrpcChannelArgumentsNativeOption>().SetUserAgentPrefix(
        user_agent_prefix);
  }

  /**
   * Set the buffer pool to be attached to the constructed channel.
   *
   * Please see the docs for `grpc::ChannelArguments::SetResourceQuota()`
   * on https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html
   * for more details.
   *
   */
  void SetResourceQuota(grpc::ResourceQuota const& resource_quota) {
    opts_.lookup<GrpcChannelArgumentsNativeOption>().SetResourceQuota(
        resource_quota);
  }

  /**
   * Set the max receive message size in bytes. -1 means unlimited.
   *
   * Please see the docs for
   * `grpc::ChannelArguments::SetMaxReceiveMessageSize()` on
   * https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html for more
   * details.
   *
   */
  void SetMaxReceiveMessageSize(int size) {
    opts_.lookup<GrpcChannelArgumentsNativeOption>().SetMaxReceiveMessageSize(
        size);
  }

  /**
   * Set the max send message size in bytes. -1 means unlimited.
   *
   * Please see the docs for grpc::ChannelArguments::SetMaxSendMessageSize()
   * on https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html
   * for more details.
   *
   */
  void SetMaxSendMessageSize(int size) {
    opts_.lookup<GrpcChannelArgumentsNativeOption>().SetMaxSendMessageSize(
        size);
  }

  /**
   * Set LB policy name.
   *
   * Please see the docs for
   * grpc::ChannelArguments::SetLoadBalancingPolicyName()
   * on https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html
   * for more details.
   *
   */
  void SetLoadBalancingPolicyName(grpc::string const& lb_policy_name) {
    opts_.lookup<GrpcChannelArgumentsNativeOption>().SetLoadBalancingPolicyName(
        lb_policy_name);
  }

  /**
   * Set service config in JSON form.
   *
   * Please see the docs for grpc::ChannelArguments::SetServiceConfigJSON()
   * on https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html
   * for more details.
   *
   */
  void SetServiceConfigJSON(grpc::string const& service_config_json) {
    opts_.lookup<GrpcChannelArgumentsNativeOption>().SetServiceConfigJSON(
        service_config_json);
  }

  /**
   * Set target name override for SSL host name checking.
   *
   * Please see the docs for grpc::ChannelArguments::SetSslTargetNameOverride()
   * on https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html
   * for more details.
   *
   */
  void SetSslTargetNameOverride(grpc::string const& name) {
    opts_.lookup<GrpcChannelArgumentsNativeOption>().SetSslTargetNameOverride(
        name);
  }

  /// Return the user agent prefix used by the library.
  static std::string UserAgentPrefix();

  /**
   * Return whether tracing is enabled for the given @p component.
   *
   * The C++ clients can log interesting events to help library and application
   * developers troubleshoot problems. This flag returns true if tracing should
   * be enabled by clients configured with this option.
   */
  bool tracing_enabled(std::string const& component) const {
    return google::cloud::internal::Contains(
        opts_.get<TracingComponentsOption>(), component);
  }

  /// Enable tracing for @p component in clients configured with this object.
  ClientOptions& enable_tracing(std::string const& component) {
    opts_.lookup<TracingComponentsOption>().insert(component);
    return *this;
  }

  /// Disable tracing for @p component in clients configured with this object.
  ClientOptions& disable_tracing(std::string const& component) {
    opts_.lookup<TracingComponentsOption>().erase(component);
    return *this;
  }

  /// Return the options for use when tracing RPCs.
  TracingOptions const& tracing_options() const {
    return opts_.get<GrpcTracingOptionsOption>();
  }

  /**
   * Maximum connection refresh period, as set via `set_max_conn_refresh_period`
   */
  std::chrono::milliseconds max_conn_refresh_period() {
    return opts_.get<MaxConnectionRefreshOption>();
  }

  /**
   * If set to a positive number, the client will refresh connections at random
   * moments not more apart from each other than this duration. This is
   * necessary to avoid all connections simultaneously expiring and causing
   * latency spikes.
   *
   * If needed it changes max_conn_refresh_period() to preserve the invariant:
   *
   * @code
   * assert(min_conn_refresh_period() <= max_conn_refresh_period())
   * @endcode
   */
  ClientOptions& set_max_conn_refresh_period(std::chrono::milliseconds period) {
    opts_.set<MaxConnectionRefreshOption>(period);
    auto& min_conn_refresh_period = opts_.lookup<MinConnectionRefreshOption>();
    min_conn_refresh_period = (std::min)(min_conn_refresh_period, period);
    return *this;
  }

  /**
   * Minimum connection refresh period, as set via `set_min_conn_refresh_period`
   */
  std::chrono::milliseconds min_conn_refresh_period() {
    return opts_.get<MinConnectionRefreshOption>();
  }

  /**
   * Configures the *minimum* connection refresh period. The library will wait
   * at least this long before attempting any refresh operation.
   *
   * If needed it changes max_conn_refresh_period() to preserve the invariant:
   *
   * @code
   * assert(min_conn_refresh_period() <= max_conn_refresh_period())
   * @endcode
   */
  ClientOptions& set_min_conn_refresh_period(std::chrono::milliseconds period) {
    opts_.set<MinConnectionRefreshOption>(period);
    auto& max_conn_refresh_period = opts_.lookup<MaxConnectionRefreshOption>();
    max_conn_refresh_period = (std::max)(max_conn_refresh_period, period);
    return *this;
  }

  /**
   * Set the number of background threads.
   *
   * @note This value is not used if `DisableBackgroundThreads()` is called.
   */
  ClientOptions& set_background_thread_pool_size(std::size_t s) {
    opts_.set<GrpcBackgroundThreadPoolSizeOption>(s);
    return *this;
  }

  std::size_t background_thread_pool_size() const {
    return opts_.get<GrpcBackgroundThreadPoolSizeOption>();
  }

  /**
   * Configure the connection to use @p cq for all background work.
   *
   * Connections need to perform background work on behalf of the application.
   * Normally they just create a background thread and a `CompletionQueue` for
   * this work, but the application may need more fine-grained control of their
   * threads. In this case the application can provide the `CompletionQueue` and
   * it assumes responsibility for creating one or more threads blocked on
   * `CompletionQueue::Run()`.
   */
  ClientOptions& DisableBackgroundThreads(
      google::cloud::CompletionQueue const& cq);

  /**
   * Backwards compatibility alias
   *
   * @deprecated Consider using `google::cloud::BackgroundThreadsFactory`
   * directly
   */
  using BackgroundThreadsFactory = ::google::cloud::BackgroundThreadsFactory;
  BackgroundThreadsFactory background_threads_factory() const;

 private:
  friend struct ClientOptionsTestTraits;
  friend Options&& internal::MakeOptions(ClientOptions&&);

  /// Return the current endpoint for instance admin RPCs.
  std::string const& instance_admin_endpoint() const {
    return opts_.get<InstanceAdminEndpointOption>();
  }

  std::string connection_pool_name_;
  Options opts_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_CLIENT_OPTIONS_H
