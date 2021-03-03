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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_REJECTS_WITH_ORDERING_KEY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_REJECTS_WITH_ORDERING_KEY_H

#include "google/cloud/pubsub/publisher_connection.h"
#include "google/cloud/pubsub/version.h"
#include <string>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

class RejectsWithOrderingKey : public pubsub::PublisherConnection {
 public:
  static std::shared_ptr<RejectsWithOrderingKey> Create(
      std::shared_ptr<pubsub::PublisherConnection> connection) {
    return std::shared_ptr<RejectsWithOrderingKey>(
        new RejectsWithOrderingKey(std::move(connection)));
  }

  ~RejectsWithOrderingKey() override = default;

  future<StatusOr<std::string>> Publish(PublishParams p) override;
  void Flush(FlushParams) override;
  void ResumePublish(ResumePublishParams p) override;

 private:
  explicit RejectsWithOrderingKey(
      std::shared_ptr<PublisherConnection> connection)
      : connection_(std::move(connection)) {}

  std::shared_ptr<PublisherConnection> connection_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_REJECTS_WITH_ORDERING_KEY_H
