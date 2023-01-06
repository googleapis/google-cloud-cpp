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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_BLOCKING_PUBLISHER_CONNECTION_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_BLOCKING_PUBLISHER_CONNECTION_IMPL_H

#include "google/cloud/pubsub/blocking_publisher_connection.h"
#include "google/cloud/pubsub/internal/publisher_stub.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/background_threads.h"
#include <memory>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class BlockingPublisherConnectionImpl
    : public pubsub::BlockingPublisherConnection {
 public:
  BlockingPublisherConnectionImpl(
      std::unique_ptr<google::cloud::BackgroundThreads> background,
      std::shared_ptr<PublisherStub> stub, Options options);
  ~BlockingPublisherConnectionImpl() override = default;

  StatusOr<std::string> Publish(PublishParams p) override;
  Options options() override;

 private:
  std::unique_ptr<google::cloud::BackgroundThreads> background_;
  std::shared_ptr<PublisherStub> stub_;
  Options options_;
};

std::shared_ptr<pubsub::BlockingPublisherConnection>
MakeTestBlockingPublisherConnection(
    Options opts, std::vector<std::shared_ptr<PublisherStub>> mocks);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_BLOCKING_PUBLISHER_CONNECTION_IMPL_H
