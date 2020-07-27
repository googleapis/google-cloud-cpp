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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_DEFAULT_ACK_HANDLER_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_DEFAULT_ACK_HANDLER_IMPL_H

#include "google/cloud/pubsub/ack_handler.h"
#include "google/cloud/pubsub/internal/subscriber_stub.h"
#include "google/cloud/pubsub/version.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

class DefaultAckHandlerImpl : public pubsub::AckHandler::Impl {
 public:
  DefaultAckHandlerImpl(google::cloud::CompletionQueue cq,
                        std::shared_ptr<pubsub_internal::SubscriberStub> s,
                        std::string subscription, std::string ack_id)
      : cq_(std::move(cq)),
        stub_(std::move(s)),
        subscription_(std::move(subscription)),
        ack_id_(std::move(ack_id)) {}

  ~DefaultAckHandlerImpl() override = default;

  void ack() override;
  void nack() override;

  std::string ack_id() const override { return ack_id_; }

 private:
  google::cloud::CompletionQueue cq_;
  std::shared_ptr<pubsub_internal::SubscriberStub> stub_;
  std::string subscription_;
  std::string ack_id_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_DEFAULT_ACK_HANDLER_IMPL_H
