// Copyright 2021 Google LLC
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

#include "google/cloud/pubsub/internal/flow_controlled_publisher_connection.h"
#include <algorithm>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {
StatusOr<std::string> RejectMessage() {
  return Status(StatusCode::kFailedPrecondition, "Publisher is full");
}
}  // namespace

future<StatusOr<std::string>> FlowControlledPublisherConnection::Publish(
    PublishParams p) {
  auto const message_size = MessageSize(p.message);
  std::unique_lock<std::mutex> lk(mu_);
  if (MakesFull(message_size)) {
    if (RejectWhenFull()) return make_ready_future(RejectMessage());
    if (BlockWhenFull()) {
      cv_.wait(lk, [this, message_size] { return !MakesFull(message_size); });
    }
  }
  ++pending_messages_;
  pending_bytes_ += message_size;
  auto w = WeakFromThis();
  auto r = child_->Publish(std::move(p));
  max_pending_messages_ = (std::max)(max_pending_messages_, pending_messages_);
  max_pending_bytes_ = (std::max)(max_pending_bytes_, pending_bytes_);
  lk.unlock();
  r = r.then([w, message_size](future<StatusOr<std::string>> f) {
    if (auto self = w.lock()) self->OnPublish(message_size);
    return f.get();
  });
  lk.lock();
  return r;
}

void FlowControlledPublisherConnection::Flush(FlushParams p) {
  return child_->Flush(std::move(p));
}

void FlowControlledPublisherConnection::ResumePublish(ResumePublishParams p) {
  return child_->ResumePublish(std::move(p));
}

void FlowControlledPublisherConnection::OnPublish(std::size_t message_size) {
  std::unique_lock<std::mutex> lk(mu_);
  --pending_messages_;
  pending_bytes_ -= message_size;
  if (IsFull()) return;  // Nothing to notify
  lk.unlock();
  cv_.notify_all();
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
