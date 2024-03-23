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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_DEFAULT_BATCH_CALLBACK_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_DEFAULT_BATCH_CALLBACK_H

#include "google/cloud/pubsub/internal/batch_callback.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/status_or.h"
#include <google/pubsub/v1/pubsub.pb.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Default implementation.
 */
class DefaultBatchCallback : public BatchCallback {
 public:
  using CallbackFunction =
      std::function<void(BatchCallback::StreamingPullResponse)>;
  DefaultBatchCallback(CallbackFunction callback,
                       std::shared_ptr<MessageCallback> message_callback)
      : callback_(std::move(callback)),
        message_callback_(std::move(message_callback)) {}
  ~DefaultBatchCallback() override = default;

  void callback(StreamingPullResponse response) override {
    callback_(std::move(response));
  };

  void message_callback(MessageCallback::ReceivedMessage m) override {
    message_callback_->message_callback(std::move(m));
  };

  void user_callback(MessageCallback::MessageAndHandler m) override {
    message_callback_->user_callback(std::move(m));
  };

  void EndMessage(std::string const& ack_id,
                  std::string const& event) override{};

 private:
  CallbackFunction callback_;
  std::shared_ptr<MessageCallback> message_callback_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_DEFAULT_BATCH_CALLBACK_H
