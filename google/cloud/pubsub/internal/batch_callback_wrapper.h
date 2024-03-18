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
#include "google/cloud/pubsub/version.h"
#include "google/cloud/status_or.h"
#include <google/pubsub/v1/pubsub.pb.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Tracing implementation.
 * */
class BatchCallbackWrapper : public BatchCallback {
 public:
  using Callback = std::function<void(StreamingPullResponse)>;

  explicit BatchCallbackWrapper(std::shared_ptr<BatchCallback> child,
                                Callback wrapper)
      : child_(std::move(child)), wrapper_(std::move(wrapper)) {}
  ~BatchCallbackWrapper() override = default;

  void callback(StreamingPullResponse response) override {
    wrapper_(response);
    child_->callback(response);
  };

  std::shared_ptr<BatchCallback> child_;
  Callback wrapper_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_BATCH_CALLBACK_WRAPPER_H
