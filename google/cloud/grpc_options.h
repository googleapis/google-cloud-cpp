// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_GRPC_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_GRPC_OPTIONS_H

#include "google/cloud/background_threads.h"
#include "google/cloud/options.h"
#include "google/cloud/tracing_options.h"
#include "google/cloud/version.h"
#include <grpcpp/grpcpp.h>
#include <set>
#include <string>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {

/**
 * The gRPC credentials used by clients configured with this object.
 */
struct GrpcCredentialOption {
  std::shared_ptr<grpc::ChannelCredentials> value;
};

/**
 * Change the gRPC endpoint.
 *
 * In almost all cases the default is the correct endpoint to use. Applications
 * may need to be changed to (1) test against a fake or simulator, or (2) to
 * use a beta or EAP version of the service.
 */
struct GrpcEndpointOption {
  std::string value;
};

/**
 * The number of transport channels to create.
 *
 * gRPC limits the number of simultaneous calls in progress on a channel to
 * 100. Increasing the number of channels thus increases the number of
 * operations that can be in progress in parallel.
 */
struct GrpcNumChannelsOption {
  int value;
};

/**
 * User-agent strings to include with each request.
 *
 * Libraries or services that use Cloud C++ clients may want to set their own
 * user-agent prefix. This can help them develop telemetry information about
 * number of users running particular versions of their system or library.
 */
struct GrpcUserAgentPrefixOption {
  std::set<std::string> value;
};

/**
 * A string-string map of arguments for `grpc::ChannelArguments::SetString`.
 *
 * This option gives users the ability to set various arguments for the
 * underlying `grpc::ChannelArguments` objects that will be created. See the
 * gRPC documentation for more details about available options.
 *
 * @see https://grpc.github.io/grpc/cpp/classgrpc_1_1_channel_arguments.html
 * @see https://grpc.github.io/grpc/core/group__grpc__arg__keys.html
 */
struct GrpcChannelArgumentsOption {
  std::map<std::string, std::string> value;
};

/**
 * Return whether tracing is enabled for the given @p component.
 *
 * The C++ clients can log interesting events to help library and application
 * developers troubleshoot problems. To see log messages (maybe lots) you can
 * enable tracing for the component that interests you. Valid components are
 * currently:
 *
 * - rpc
 * - rpc-streams
 */
struct GrpcTracingComponentsOption {
  std::set<std::string> value;
};

/**
 * The `TracingOptions` to use when printing grpc protocol buffer messages.
 */
struct GrpcTracingOptionsOption {
  TracingOptions value;
};

/**
 * Changes the `BackgroundThreadsFactory`.
 *
 * Connections need to perform background work on behalf of the application.
 * Normally they just create a background thread and a `CompletionQueue` for
 * this work, but the application may need more fine-grained control of their
 * threads. In this case the application can provide its own
 * `BackgroundThreadsFactory` and it assumes responsibility for creating one or
 * more threads blocked on its `CompletionQueue::Run()`.
 */
using BackgroundThreadsFactory =
    std::function<std::unique_ptr<BackgroundThreads>()>;
struct GrpcBackgroundThreadsOption {
  BackgroundThreadsFactory value;
};

/**
 * Set the number of background threads.
 *
 * @note this value is not used if `DisableBackgroundThreads()` is called.
 */
/* ConnectionOptions& set_background_thread_pool_size(std::size_t s) { */
/*   background_thread_pool_size_ = s; */
/*   return *this; */
/* } */
/* std::size_t background_thread_pool_size() const { */
/*   return background_thread_pool_size_; */
/* } */

/* ConnectionOptions& DisableBackgroundThreads( */
/*     google::cloud::CompletionQueue const& cq) { */
/*   background_threads_factory_ = [cq] { */
/*     return
 * absl::make_unique<internal::CustomerSuppliedBackgroundThreads>(cq); */
/*   }; */
/*   return *this; */
/* } */

/* using BackgroundThreadsFactory = */
/*     std::function<std::unique_ptr<BackgroundThreads>()>; */
/* BackgroundThreadsFactory background_threads_factory() const { */
/*   if (background_threads_factory_) return background_threads_factory_; */
/*   auto const s = background_thread_pool_size_; */
/*   return [s] { return internal::DefaultBackgroundThreads(s); }; */
/* } */

namespace internal {

/// Creates a new `grpc::ChannelArguments` configured with @p opts.
grpc::ChannelArguments MakeChannelArguments(Options const& opts);

}  // namespace internal

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_GRPC_OPTIONS_H
