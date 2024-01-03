// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_PUBLISHER_STUB_FACTORY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_PUBLISHER_STUB_FACTORY_H

#include "google/cloud/pubsub/internal/publisher_stub.h"
#include "google/cloud/options.h"
#include <memory>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A function that creates a PublisherStub using a pre-configured channel.
 */
using BasePublisherStubFactory = std::function<std::shared_ptr<PublisherStub>(
    std::shared_ptr<grpc::Channel>)>;

/**
 * Creates a PublisherStub configured with @p cq and @p options.
 *
 * By default, a PublisherRoundRobinStub is created using the number of
 * GrpcNumChannelsOption.
 */
std::shared_ptr<PublisherStub> MakeRoundRobinPublisherStub(
    google::cloud::CompletionQueue cq, Options const& options);

/**
 * Creates a test PublisherStub configured with @p cq and @p options and @p
 * mocks.
 *
 * Used for testing the stubs at the connection layer.
 */
std::shared_ptr<PublisherStub> MakeTestPublisherStub(
    google::cloud::CompletionQueue cq, Options const& options,
    std::vector<std::shared_ptr<PublisherStub>> mocks);

/**
 * Creates a test PublisherStub configured with @p cq and @p options and @p
 * base_factory.
 *
 * Used for unit testing to create decorated stubs. Accepts a stub factory so we
 * can inject mock stubs in our unit tests.
 */
std::shared_ptr<PublisherStub> CreateDecoratedStubs(
    google::cloud::CompletionQueue cq, Options const& options,
    BasePublisherStubFactory const& base_factory);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_PUBLISHER_STUB_FACTORY_H
