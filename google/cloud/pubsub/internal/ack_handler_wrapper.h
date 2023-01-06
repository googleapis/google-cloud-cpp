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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_ACK_HANDLER_WRAPPER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_ACK_HANDLER_WRAPPER_H

#include "google/cloud/pubsub/ack_handler.h"
#include "google/cloud/pubsub/exactly_once_ack_handler.h"
#include "google/cloud/version.h"
#include <string>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class AckHandlerWrapper : public pubsub::AckHandler::Impl {
 public:
  explicit AckHandlerWrapper(
      std::unique_ptr<pubsub::ExactlyOnceAckHandler::Impl> impl)
      : impl_(std::move(impl)) {}
  AckHandlerWrapper(std::unique_ptr<pubsub::ExactlyOnceAckHandler::Impl> impl,
                    std::string message_id)
      : impl_(std::move(impl)), message_id_(std::move(message_id)) {}
  ~AckHandlerWrapper() override = default;

  void ack() override;
  void nack() override;
  std::int32_t delivery_attempt() const override;

 private:
  std::unique_ptr<pubsub::ExactlyOnceAckHandler::Impl> impl_;
  std::string message_id_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_ACK_HANDLER_WRAPPER_H
