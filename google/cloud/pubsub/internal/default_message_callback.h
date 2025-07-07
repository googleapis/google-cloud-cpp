// Copyright 2024 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_DEFAULT_MESSAGE_CALLBACK_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_DEFAULT_MESSAGE_CALLBACK_H

#include "google/cloud/pubsub/internal/message_callback.h"
#include "google/cloud/pubsub/version.h"
#include "google/pubsub/v1/pubsub.pb.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Default implementation.
 */
class DefaultMessageCallback : public MessageCallback {
 public:
  using Callback = std::function<void(
      pubsub::Message, std::unique_ptr<pubsub::ExactlyOnceAckHandler::Impl>)>;

  explicit DefaultMessageCallback(Callback callback)
      : callback_(std::move(callback)) {}
  ~DefaultMessageCallback() override = default;

  void user_callback(MessageAndHandler m) override {
    callback_(std::move(m.message), std::move(m.ack_handler));
  };

 private:
  Callback callback_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_DEFAULT_MESSAGE_CALLBACK_H
