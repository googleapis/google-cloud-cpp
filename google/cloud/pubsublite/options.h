// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_OPTIONS_H

#include "google/cloud/pubsub/message.h"
#include "google/cloud/pubsublite/internal/resumable_async_streaming_read_write_rpc.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/status/status.h"
#include <google/cloud/pubsublite/v1/common.pb.h>
#include <chrono>
#include <functional>

namespace google {
namespace cloud {
namespace pubsublite {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Converts a Google Cloud Pub/Sub `Message` into a Google Cloud Pub/Sub Lite
 * protobuf `PubSubMessage`.
 */
using PublishMessageTransformer =
    std::function<StatusOr<google::cloud::pubsublite::v1::PubSubMessage>(
        google::cloud::pubsub::Message)>;

struct PublishMessageTransformerOption {
  using Type = PublishMessageTransformer;
};

/**
 * Handler for if the publisher connection shuts down with a non-`kOk` status.
 */
using FailureHandler = std::function<void(absl::Status)>;

struct FailureHandlerOption {
  using Type = FailureHandler;
};

struct MaximumBatchMessageCountOption {
  using Type = std::int64_t;
};

struct MaximumBatchBytesOption {
  using Type = std::int64_t;
};

struct PublishFlushAlarmPeriodOption {
  using Type = std::chrono::milliseconds;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_OPTIONS_H
