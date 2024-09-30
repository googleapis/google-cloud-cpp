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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_GRPC_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_GRPC_OPTIONS_H

#include "google/cloud/background_threads.h"
#include "google/cloud/common_options.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/options.h"
#include "google/cloud/tracing_options.h"
#include "google/cloud/version.h"
#include <grpcpp/grpcpp.h>
#include <map>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * The gRPC credentials used by clients configured with this object.
 *
 * @ingroup options
 */
struct GrpcCredentialOption {
  using Type = std::shared_ptr<grpc::ChannelCredentials>;
};

/**
 * The gRPC compression algorithm used by clients/operations configured
 * with this object.
 *
 * @ingroup options
 */
struct GrpcCompressionAlgorithmOption {
  using Type = grpc_compression_algorithm;
};

/**
 * The number of transport channels to create.
 *
 * gRPC limits the number of simultaneous calls in progress on a channel to
 * 100. Increasing the number of channels thus increases the number of
 * operations that can be in progress in parallel.
 *
 * @note This option only applies when passed to the following functions:
 * - `bigtable::MakeDataConnection()`
 * - `pubsub::MakePublisherConnection()`
 * - `pubsub::MakeSubscriberConnection()`
 * - `spanner::MakeConnection()`
 * - `storage::MakeGrpcClient()`
 *
 * @ingroup options
 */
struct GrpcNumChannelsOption {
  using Type = int;
};

/**
 * A string-string map of arguments for `grpc::ChannelArguments::SetString`.
 *
 * This option gives users the ability to set various arguments for the
 * underlying `grpc::ChannelArguments` objects that will be created. See the
 * gRPC documentation for more details about available channel arguments.
 *
 * @note Our library will always start with the native object from
 * `GrpcChannelArgumentsNativeOption`, then add the channel arguments from this
 * option. Users are cautioned not to set the same channel argument to different
 * values in different options as gRPC will use the first value set for some
 * channel arguments, and the last value set for others.
 *
 * @see https://grpc.github.io/grpc/cpp/classgrpc_1_1_channel_arguments.html
 * @see https://grpc.github.io/grpc/core/group__grpc__arg__keys.html
 *
 * @ingroup options
 */
struct GrpcChannelArgumentsOption {
  using Type = std::map<std::string, std::string>;
};

/**
 * The native `grpc::ChannelArguments` object.
 *
 * This option gives users full control over the `grpc::ChannelArguments`
 * objects that will be created. See the gRPC documentation for more details
 * about available channel arguments.
 *
 * @note Our library will always start with the native object, then add in the
 * channel arguments from `GrpcChannelArgumentsOption`, then add the user agent
 * prefix from `UserAgentProductsOption`. Users are cautioned not to set the
 * same channel argument to different values in different options as gRPC will
 * use the first value set for some channel arguments, and the last value set
 * for others.
 *
 * @see https://grpc.github.io/grpc/cpp/classgrpc_1_1_channel_arguments.html
 * @see https://grpc.github.io/grpc/core/group__grpc__arg__keys.html
 *
 * @ingroup options
 */
struct GrpcChannelArgumentsNativeOption {
  using Type = grpc::ChannelArguments;
};

/**
 * The `TracingOptions` to use when printing grpc protocol buffer messages.
 *
 * @ingroup options
 */
struct GrpcTracingOptionsOption {
  using Type = TracingOptions;
};

/**
 * The size of the background thread pool
 *
 * These threads are created by the client library to run a `CompletionQueue`
 * which performs background work for gRPC.
 *
 * @note `GrpcBackgroundThreadPoolSizeOption`, `GrpcCompletionQueueOption`, and
 *     `GrpcBackgroundThreadsFactoryOption` are mutually exclusive. This option
 *     will be ignored if either `GrpcCompletionQueueOption` or
 *     `GrpcBackgroundThreadsFactoryOption` are set.
 *
 * @ingroup options
 */
struct GrpcBackgroundThreadPoolSizeOption {
  using Type = std::size_t;
};

/**
 * The `CompletionQueue` to use for background gRPC work.
 *
 * If this option is set, the library will use the supplied `CompletionQueue`
 * instead of its own. The caller is responsible for making sure there are
 * thread(s) servicing this `CompletionQueue`. The client library will not
 * create any background threads or attempt to call `CompletionQueue::Run()`.
 *
 * @note `GrpcBackgroundThreadPoolSizeOption`, `GrpcCompletionQueueOption`, and
 *     `GrpcBackgroundThreadsFactoryOption` are mutually exclusive.
 *
 * @ingroup options
 */
struct GrpcCompletionQueueOption {
  using Type = CompletionQueue;
};

using BackgroundThreadsFactory =
    std::function<std::unique_ptr<BackgroundThreads>()>;

/**
 * Changes the `BackgroundThreadsFactory`.
 *
 * Connections need to perform background work on behalf of the application.
 * Normally they just create a background thread and a `CompletionQueue` for
 * this work, but the application may need more fine-grained control of their
 * threads.
 *
 * In this case the application can provide its own `BackgroundThreadsFactory`
 * and it assumes responsibility for creating one or more threads blocked on its
 * `CompletionQueue::Run()`.
 *
 * @note `GrpcBackgroundThreadPoolSizeOption`, `GrpcCompletionQueueOption`, and
 *     `GrpcBackgroundThreadsFactoryOption` are mutually exclusive. This option
 *     will be ignored if `GrpcCompletionQueueOption` is set.
 *
 * @ingroup options
 */
struct GrpcBackgroundThreadsFactoryOption {
  using Type = BackgroundThreadsFactory;
};

/**
 * A list of all the gRPC options.
 */
using GrpcOptionList =
    OptionList<GrpcCredentialOption, GrpcCompressionAlgorithmOption,
               GrpcNumChannelsOption, GrpcChannelArgumentsOption,
               GrpcChannelArgumentsNativeOption, GrpcTracingOptionsOption,
               GrpcBackgroundThreadPoolSizeOption, GrpcCompletionQueueOption,
               GrpcBackgroundThreadsFactoryOption>;

namespace internal {

/**
 * A generic function to directly configure a gRPC context.
 *
 * This function takes effect before the context is used to make any requests.
 *
 * @warning It is NOT recommended to call `set_auth_context()` or
 *     `set_credentials()` directly on the context. Instead, use the Google
 *     Unified Auth Credentials library, via
 *     #google::cloud::UnifiedCredentialsOption.
 *
 * @warning `MergeOptions()` will simply select the preferred function, rather
 *     than merging the behavior of the preferred and alternate functions.
 */
struct GrpcSetupOption {
  using Type = std::function<void(grpc::ClientContext&)>;
};

/**
 * A generic function to directly configure a gRPC context for polling
 * long-running operations.
 *
 * This function takes effect before the context is used to make any poll or
 * cancel requests for long-running operations.
 *
 * @warning It is NOT recommended to call `set_auth_context()` or
 *     `set_credentials()` directly on the context. Instead, use the Google
 *     Unified Auth Credentials library, via
 *     #google::cloud::UnifiedCredentialsOption.
 *
 * @warning `MergeOptions()` will simply select the preferred function, rather
 *     than merging the behavior of the preferred and alternate functions.
 */
struct GrpcSetupPollOption {
  using Type = std::function<void(grpc::ClientContext&)>;
};

/// Configure the metadata in @p context.
void SetMetadata(grpc::ClientContext& context, Options const& options,
                 std::multimap<std::string, std::string> const& fixed_metadata,
                 std::string const& api_client_header);

/// Configure the ClientContext using options.
void ConfigureContext(grpc::ClientContext& context, Options const& opts);

/// Configure the ClientContext for polling operations using options.
void ConfigurePollContext(grpc::ClientContext& context, Options const& opts);

/// Creates the value for GRPC_ARG_HTTP_PROXY based on @p config.
std::string MakeGrpcHttpProxy(ProxyConfig const& config);

/// Creates a new `grpc::ChannelArguments` configured with @p opts.
grpc::ChannelArguments MakeChannelArguments(Options const& opts);

/// Helper function to extract the first instance of an integer channel argument
absl::optional<int> GetIntChannelArgument(grpc::ChannelArguments const& args,
                                          std::string const& key);

/// Helper function to extract the first instance of a string channel argument
absl::optional<std::string> GetStringChannelArgument(
    grpc::ChannelArguments const& args, std::string const& key);

/**
 * Returns a factory for generating `BackgroundThreads`.
 *
 * If `GrpcBackgroundThreadsFactoryOption` is unset, it will return a thread
 * pool of size `GrpcBackgroundThreadPoolSizeOption`.
 */
BackgroundThreadsFactory MakeBackgroundThreadsFactory(Options const& opts = {});

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_GRPC_OPTIONS_H
