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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_PARTITION_PUBLISHER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_PARTITION_PUBLISHER_H

#include "google/cloud/pubsublite/internal/publisher.h"
#include "google/cloud/pubsublite/internal/resumable_async_streaming_read_write_rpc.h"
#include <google/cloud/pubsublite/v1/publisher.proto.h>
#include "absl/functional/function_ref.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

class PartitionPublisherImpl : public Publisher<Cursor> {
 public:
  explicit PartitionPublisherImpl(
      absl::FunctionRef<std::unique_ptr<ResumableAsyncStreamingReadWriteRpc>(
          StreamInitializer)>
          resumable_stream_factory)
      : resumable_stream_{resumable_stream_factory(&Initializer)} {}

  future<Status> Start() {
    // TODO(18suresha) implement
    return make_ready_future(Status());
  }

  future<StatusOr<PublishReturnType>> Publish(PubSubMessage m) {
    // TODO(18suresha) implement
    return make_ready_future(StatusOr<PublishReturnType>(Status()));
  }

  void Flush() {
    // TODO(18suresha) implement
  }

  future<void> Shutdown() {
    // TODO(18suresha) implement
    return make_ready_future();
  }

 private:
  using UnderlyingStream =
      std::unique_ptr<AsyncStreamingReadWriteRpc<MessagePublishRequest,
                                                 MessagePublishResponse>>;

  future<StatusOr<UnderlyingStream>> Initializer(UnderlyingStream stream) {
    // TODO(18suresha) implement
    return std::move(stream);
  }

  ResumableAsyncStreamingReadWriteRpc<MessagePublishRequest,
                                      MessagePublishResponse>
      resumable_stream_;
};

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_PARTITION_PUBLISHER_H
