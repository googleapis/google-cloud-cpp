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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_BATCH_CALLBACK_WRAPPER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_BATCH_CALLBACK_WRAPPER_H

#include "google/cloud/pubsub/internal/batch_callback.h"
#include "google/cloud/pubsub/internal/message_callback.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/status_or.h"
#include "google/pubsub/v1/pubsub.pb.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Tracing implementation.
 */
class BatchCallbackWrapper : public BatchCallback {
 public:
  using Callback = std::function<void(StreamingPullResponse)>;
  using MessageCallback = std::function<void(ReceivedMessage)>;

  explicit BatchCallbackWrapper(std::shared_ptr<BatchCallback> child,
                                Callback wrapper)
      : child_(std::move(child)), wrapper_(std::move(wrapper)) {}
  explicit BatchCallbackWrapper(std::shared_ptr<BatchCallback> child,
                                MessageCallback wrapper)
      : child_(std::move(child)), message_wrapper_(std::move(wrapper)) {}
  ~BatchCallbackWrapper() override = default;

  void callback(StreamingPullResponse response) override {
    child_->callback(response);
    wrapper_(response);
  }

  void message_callback(ReceivedMessage m) override {
    child_->message_callback(m);
    message_wrapper_(m);
  }

  void user_callback(
      pubsub_internal::MessageCallback::MessageAndHandler m) override {
    child_->user_callback(std::move(m));
  }

  void StartConcurrencyControl(std::string const& ack_id) override {
    child_->StartConcurrencyControl(ack_id);
  }
  void EndConcurrencyControl(std::string const& ack_id) override {
    child_->EndConcurrencyControl(ack_id);
  }

  void StartScheduler(std::string const& ack_id) override {
    child_->StartScheduler(ack_id);
  }
  void EndScheduler(std::string const& ack_id) override {
    child_->EndScheduler(ack_id);
  }

  Span StartModackSpan(
      google::pubsub::v1::ModifyAckDeadlineRequest const& request) override {
    return child_->StartModackSpan(request);
  }
  void EndModackSpan(Span span) override { child_->EndModackSpan(span); }

  void AckStart(std::string const& ack_id) override {
    child_->AckStart(ack_id);
  }
  void AckEnd(std::string const& ack_id) override { child_->AckEnd(ack_id); }

  void NackStart(std::string const& ack_id) override {
    child_->NackStart(ack_id);
  }
  void NackEnd(std::string const& ack_id) override { child_->NackEnd(ack_id); }

  void ModackStart(std::string const& ack_id) override {
    child_->ModackStart(ack_id);
  }
  void ModackEnd(std::string const& ack_id) override {
    child_->ModackEnd(ack_id);
  }

  void ExpireMessage(std::string const& ack_id) override {
    child_->ExpireMessage(ack_id);
  }

 private:
  std::shared_ptr<BatchCallback> child_;
  MessageCallback message_wrapper_ = [](ReceivedMessage const&) {};
  Callback wrapper_ = [](StreamingPullResponse const&) {};
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_BATCH_CALLBACK_WRAPPER_H
