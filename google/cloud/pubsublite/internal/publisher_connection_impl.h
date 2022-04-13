// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_PUBLISHER_CONNECTION_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_PUBLISHER_CONNECTION_IMPL_H

#include "google/cloud/pubsub/publisher_connection.h"
#include "google/cloud/pubsublite/internal/publisher.h"
#include "google/cloud/pubsublite/internal/service_composite.h"
#include "google/cloud/pubsublite/message_metadata.h"
#include "google/cloud/pubsublite/options.h"

namespace google {
namespace cloud {
namespace pubsublite_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A connection implementation for publishing messages to a single `Topic`.
 */
class PublisherConnectionImpl
    : public google::cloud::pubsub::PublisherConnection {
 public:
  PublisherConnectionImpl(
      std::unique_ptr<google::cloud::pubsublite_internal::Publisher<
          google::cloud::pubsublite::MessageMetadata>>
          publisher,
      google::cloud::pubsublite::PublishMessageTransformer transformer,
      google::cloud::pubsublite::FailureHandler const& failure_handler);

  ~PublisherConnectionImpl() override;

  future<StatusOr<std::string>> Publish(PublishParams p) override;

  void Flush(FlushParams) override;

  void ResumePublish(ResumePublishParams) override{};

 private:
  std::unique_ptr<google::cloud::pubsublite_internal::Publisher<
      google::cloud::pubsublite::MessageMetadata>> const publisher_;
  google::cloud::pubsublite_internal::ServiceComposite service_composite_;
  google::cloud::pubsublite::PublishMessageTransformer const
      message_transformer_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_PUBLISHER_CONNECTION_IMPL_H
