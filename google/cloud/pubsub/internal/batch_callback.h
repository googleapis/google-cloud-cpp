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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_BATCH_CALLBACK_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_BATCH_CALLBACK_H

#include "google/cloud/pubsub/internal/message_callback.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/status_or.h"
#include <google/pubsub/v1/pubsub.pb.h>
#include <string>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Define the interface to receive message batches from Cloud Pub/Sub via the
 * Streaming Pull.
 */
class BatchCallback {
 public:
  virtual ~BatchCallback() = default;

  // Define the struct to store the response from Cloud Pub/Sub.
  struct StreamingPullResponse {
    // A batch of messages received.
    StatusOr<google::pubsub::v1::StreamingPullResponse> response;
  };

  virtual void callback(StreamingPullResponse response) = 0;
  virtual void message_callback(MessageCallback::ReceivedMessage m) = 0;
  virtual void user_callback(MessageCallback::MessageAndHandler m) = 0;

  virtual void StartConcurrencyControl(std::string const& ack_id) = 0;
  virtual void EndConcurrencyControl(std::string const& ack_id) = 0;

  virtual void AckStart(std::string const& ack_id) = 0;
  virtual void AckEnd(std::string const& ack_id) = 0;

  virtual void NackStart(std::string const& ack_id) = 0;
  virtual void NackEnd(std::string const& ack_id) = 0;

  virtual void ModackStart(std::string const& ack_id) = 0;
  virtual void ModackEnd(std::string const& ack_id) = 0;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_BATCH_CALLBACK_H
