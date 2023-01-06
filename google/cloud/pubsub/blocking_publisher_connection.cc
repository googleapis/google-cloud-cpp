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

#include "google/cloud/pubsub/blocking_publisher_connection.h"
#include "google/cloud/pubsub/internal/blocking_publisher_connection_impl.h"
#include "google/cloud/pubsub/internal/defaults.h"
#include "google/cloud/pubsub/internal/publisher_stub_factory.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/credentials.h"
#include <memory>

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

BlockingPublisherConnection::~BlockingPublisherConnection() = default;

// NOLINTNEXTLINE(performance-unnecessary-value-param)
StatusOr<std::string> BlockingPublisherConnection::Publish(PublishParams) {
  return Status{StatusCode::kUnimplemented, "needs-override"};
}

std::shared_ptr<BlockingPublisherConnection> MakeBlockingPublisherConnection(
    Options opts) {
  internal::CheckExpectedOptions<CommonOptionList, GrpcOptionList,
                                 UnifiedCredentialsOptionList, PolicyOptionList,
                                 PublisherOptionList>(opts, __func__);
  opts = pubsub_internal::DefaultPublisherOptions(std::move(opts));
  auto background = internal::MakeBackgroundThreadsFactory(opts)();
  auto stub =
      pubsub_internal::MakeRoundRobinPublisherStub(background->cq(), opts);
  return std::make_shared<pubsub_internal::BlockingPublisherConnectionImpl>(
      std::move(background), std::move(stub), std::move(opts));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
