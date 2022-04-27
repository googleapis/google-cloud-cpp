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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_CONTAINING_PUBLISHER_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_CONTAINING_PUBLISHER_CONNECTION_H

#include "google/cloud/pubsub/publisher.h"
#include <memory>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class ContainingPublisherConnection
    : public google::cloud::pubsub::PublisherConnection {
 public:
  ContainingPublisherConnection(std::shared_ptr<BackgroundThreads> background,
                                std::shared_ptr<PublisherConnection> child)
      : background_(std::move(background)), child_(std::move(child)) {}

  ~ContainingPublisherConnection() override = default;

  future<StatusOr<std::string>> Publish(PublishParams p) override {
    return child_->Publish(std::move(p));
  }
  void Flush(FlushParams p) override { child_->Flush(std::move(p)); }
  void ResumePublish(ResumePublishParams p) override {
    child_->ResumePublish(std::move(p));
  }

 private:
  std::shared_ptr<BackgroundThreads> background_;
  std::shared_ptr<PublisherConnection> child_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_CONTAINING_PUBLISHER_CONNECTION_H
