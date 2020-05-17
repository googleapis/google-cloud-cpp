// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_CONNECTION_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_CONNECTION_OPTIONS_H

#include "google/cloud/completion_queue.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/status_or.h"
#include "google/cloud/tracing_options.h"
#include <grpcpp/grpcpp.h>
#include <functional>
#include <memory>
#include <set>
#include <string>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
std::set<std::string> DefaultTracingComponents();
TracingOptions DefaultTracingOptions();
std::unique_ptr<BackgroundThreads> DefaultBackgroundThreads();
}  // namespace internal

/**
 * The configuration parameters for client connections.
 */
template <typename ConnectionTraits>
class ConnectionOptions {
 public:
  /// The default options, using `grpc::GoogleDefaultCredentials()`.
  ConnectionOptions() : ConnectionOptions(grpc::GoogleDefaultCredentials()) {}

  /// The default options, using an explicit credentials object.
  explicit ConnectionOptions(
      std::shared_ptr<grpc::ChannelCredentials> credentials)
      : credentials_(std::move(credentials)),
        endpoint_(ConnectionTraits::default_endpoint()),
        num_channels_(ConnectionTraits::default_num_channels()),
        tracing_components_(internal::DefaultTracingComponents()),
        tracing_options_(internal::DefaultTracingOptions()),
        user_agent_prefix_(ConnectionTraits::user_agent_prefix()),
        background_threads_factory_(internal::DefaultBackgroundThreads) {}

  /// Change the gRPC credentials value.
  ConnectionOptions& set_credentials(
      // NOLINTNEXTLINE(performance-unnecessary-value-param) TODO(#4112)
      std::shared_ptr<grpc::ChannelCredentials> v) {
    credentials_ = std::move(v);
    return *this;
  }

  /// The gRPC credentials used by clients configured with this object.
  std::shared_ptr<grpc::ChannelCredentials> credentials() const {
    return credentials_;
  }

  /**
   * Change the gRPC endpoint.
   *
   * In almost all cases the default is the correct endpoint to use.
   * Applications may need to be changed to (1) test against a fake or
   * simulator, or (2) to use a beta or EAP version of the service.
   *
   * The default value is set by `ConnectionTraits::default_endpoint()`.
   */
  // NOLINTNEXTLINE(performance-unnecessary-value-param) TODO(#4112)
  ConnectionOptions& set_endpoint(std::string v) {
    endpoint_ = std::move(v);
    return *this;
  }

  /// The endpoint used by clients configured with this object.
  std::string const& endpoint() const { return endpoint_; }

  /**
   * The number of transport channels to create.
   *
   * Some transports limit the number of simultaneous calls in progress on a
   * channel (for gRPC the limit is 100). Increasing the number of channels
   * thus increases the number of operations that can be in progress in
   * parallel.
   *
   * The default value is set by `ConnectionTraits::default_num_channels()`.
   */
  int num_channels() const { return num_channels_; }

  /// Set the value for `num_channels()`.
  ConnectionOptions& set_num_channels(int num_channels) {
    num_channels_ = num_channels;
    return *this;
  }

  /**
   * Return whether tracing is enabled for the given @p component.
   *
   * The C++ clients can log interesting events to help library and application
   * developers troubleshoot problems. This flag returns true if tracing should
   * be enabled by clients configured with this option.
   */
  bool tracing_enabled(std::string const& component) const {
    return tracing_components_.find(component) != tracing_components_.end();
  }

  /// Enable tracing for @p component in clients configured with this object.
  ConnectionOptions& enable_tracing(std::string const& component) {
    tracing_components_.insert(component);
    return *this;
  }

  /// Disable tracing for @p component in clients configured with this object.
  ConnectionOptions& disable_tracing(std::string const& component) {
    tracing_components_.erase(component);
    return *this;
  }

  /// Return the options for use when tracing RPCs.
  TracingOptions const& tracing_options() const { return tracing_options_; }

  /**
   * Define the gRPC channel domain for clients configured with this object.
   *
   * In some cases applications may want to use a separate gRPC connections for
   * different clients. gRPC may share the connection used by separate channels
   * created with the same configuration. The client objects created with this
   * object will create gRPC channels configured with
   * `grpc.channel_pooling_domain` set to the value returned by
   * `channel_pool_domain()`. gRPC channels with different values for
   * `grpc.channel_pooling_domain` are guaranteed to use different connections.
   * Note that there is no guarantee that channels with the same value will have
   * the same connection though.
   *
   * This option might be useful for applications that want to segregate traffic
   * for whatever reason.
   */
  std::string const& channel_pool_domain() const {
    return channel_pool_domain_;
  }

  /// Set the value for `channel_pool_domain()`.
  // NOLINTNEXTLINE(performance-unnecessary-value-param) TODO(#4112)
  ConnectionOptions& set_channel_pool_domain(std::string v) {
    channel_pool_domain_ = std::move(v);
    return *this;
  }

  /**
   * Prepend @p prefix to the user-agent string.
   *
   * Libraries or services that use Cloud C++ clients may want to set their own
   * user-agent prefix. This can help them develop telemetry information about
   * number of users running particular versions of their system or library.
   */
  ConnectionOptions& add_user_agent_prefix(std::string prefix) {
    prefix += " ";
    prefix += user_agent_prefix_;
    user_agent_prefix_ = std::move(prefix);
    return *this;
  }

  /// Return the current value for the user agent string.
  std::string const& user_agent_prefix() const { return user_agent_prefix_; }

  /**
   * Create a new `grpc::ChannelArguments` configured with the options in this
   * object.
   *
   * The caller would typically pass this argument to
   * `grpc::CreateCustomChannel` and create one more more gRPC channels.
   */
  grpc::ChannelArguments CreateChannelArguments() const {
    grpc::ChannelArguments channel_arguments;

    if (!channel_pool_domain().empty()) {
      // To get a different channel pool one just needs to set any channel
      // parameter to a different value. Newer versions of gRPC include a macro
      // for this purpose (GRPC_ARG_CHANNEL_POOL_DOMAIN). As we are compiling
      // against older versions in some cases, we use the actual value.
      channel_arguments.SetString("grpc.channel_pooling_domain",
                                  channel_pool_domain());
    }
    channel_arguments.SetUserAgentPrefix(user_agent_prefix());
    return channel_arguments;
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
  ConnectionOptions& DisableBackgroundThreads(
      google::cloud::CompletionQueue const& cq) {
    background_threads_factory_ = [cq] {
      return absl::make_unique<internal::CustomerSuppliedBackgroundThreads>(cq);
    };
    return *this;
  }

  using BackgroundThreadsFactory =
      std::function<std::unique_ptr<BackgroundThreads>()>;
  BackgroundThreadsFactory background_threads_factory() const {
    return background_threads_factory_;
  }

 private:
  std::shared_ptr<grpc::ChannelCredentials> credentials_;
  std::string endpoint_;
  int num_channels_;
  std::set<std::string> tracing_components_;
  TracingOptions tracing_options_;
  std::string channel_pool_domain_;

  std::string user_agent_prefix_;
  BackgroundThreadsFactory background_threads_factory_;
};

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_CONNECTION_OPTIONS_H
