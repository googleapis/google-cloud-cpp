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

#include "google/cloud/pubsub/internal/default_ack_handler_impl.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

void DefaultAckHandlerImpl::ack() {
  grpc::ClientContext context;
  google::pubsub::v1::AcknowledgeRequest request;
  request.set_subscription(std::move(subscription_));
  request.add_ack_ids(std::move(ack_id_));
  (void)stub_->Acknowledge(context, request);
}

void DefaultAckHandlerImpl::nack() {
  grpc::ClientContext context;
  google::pubsub::v1::ModifyAckDeadlineRequest request;
  request.set_subscription(std::move(subscription_));
  request.add_ack_ids(std::move(ack_id_));
  request.set_ack_deadline_seconds(0);
  (void)stub_->ModifyAckDeadline(context, request);
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
