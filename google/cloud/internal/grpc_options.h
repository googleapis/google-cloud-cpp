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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_GRPC_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_GRPC_OPTIONS_H

#include "google/cloud/background_threads.h"
#include "google/cloud/internal/options.h"
#include "google/cloud/tracing_options.h"
#include "google/cloud/version.h"
#include <grpcpp/grpcpp.h>
#include <map>
#include <string>
#include <tuple>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {

namespace internal {

/**
 * The gRPC credentials used by clients configured with this object.
 */
struct GrpcCredentialOption {
  using Type = std::shared_ptr<grpc::ChannelCredentials>;
};

/**
 * The number of transport channels to create.
 *
 * gRPC limits the number of simultaneous calls in progress on a channel to
 * 100. Increasing the number of channels thus increases the number of
 * operations that can be in progress in parallel.
 */
struct GrpcNumChannelsOption {
  using Type = int;
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
  using Type = std::map<std::string, std::string>;
};

/**
 * The `TracingOptions` to use when printing grpc protocol buffer messages.
 */
struct GrpcTracingOptionsOption {
  using Type = TracingOptions;
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
struct GrpcBackgroundThreadsFactoryOption {
  using Type = BackgroundThreadsFactory;
};

}  // namespace internal

namespace internal {

/// Creates a new `grpc::ChannelArguments` configured with @p opts.
grpc::ChannelArguments MakeChannelArguments(Options const& opts);

/// Returns a factory to use if `GrpcBackgroundThreadsFactoryOption` is unset.
std::unique_ptr<BackgroundThreads> DefaultBackgroundThreadsFactory();

/**
 * A list of all the options in this file.
 *
 * This is intended to be used with `internal::CheckExpectedOptions<T>()` to
 * make it easy to specify groups of options as allowed/expected.
 *
 * @code
 * Options opts;
 * opts.set<GrpcCredentialOption>(...);
 * internal::CheckExpectedOptions<internal::GrpcOptions>(
 *     opts, "some factory function");
 * @endcode
 */
using GrpcOptions =
    std::tuple<GrpcCredentialOption, GrpcNumChannelsOption,
               GrpcChannelArgumentsOption, GrpcTracingOptionsOption,
               GrpcBackgroundThreadsFactoryOption>;

}  // namespace internal

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_GRPC_OPTIONS_H
