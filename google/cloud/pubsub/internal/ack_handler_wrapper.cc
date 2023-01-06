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

#include "google/cloud/pubsub/internal/ack_handler_wrapper.h"
#include "google/cloud/log.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

void AckHandlerWrapper::ack() {
  auto f = impl_->ack();
  if (message_id_.empty()) return;
  f.then([id = std::move(message_id_)](auto f) {
    auto status = f.get();
    GCP_LOG(WARNING) << "error while trying to ack(), status=" << status
                     << ", message_id=" << id;
  });
}

void AckHandlerWrapper::nack() {
  auto f = impl_->nack();
  if (message_id_.empty()) return;
  f.then([id = std::move(message_id_)](auto f) {
    auto status = f.get();
    GCP_LOG(WARNING) << "error while trying to nack(), status=" << status
                     << ", message_id=" << id;
  });
}

std::int32_t AckHandlerWrapper::delivery_attempt() const {
  return impl_->delivery_attempt();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
