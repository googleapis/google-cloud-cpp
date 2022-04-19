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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_COMMIT_SUBSCRIBER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_COMMIT_SUBSCRIBER_H

#include "google/cloud/pubsublite/internal/alarm_registry.h"
#include "google/cloud/pubsublite/internal/batching_options.h"
#include "google/cloud/pubsublite/internal/publisher.h"
#include "google/cloud/pubsublite/internal/resumable_async_streaming_read_write_rpc.h"
#include "google/cloud/pubsublite/internal/service_composite.h"
#include "absl/functional/function_ref.h"
#include <google/cloud/pubsublite/v1/cursor.pb.h>
#include <deque>
#include <mutex>

namespace google {
namespace cloud {
namespace pubsublite_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class CommitSubscriber : Service {
 private:
  using ResumableStream = ResumableAsyncStreamingReadWriteRpc<
      google::cloud::pubsublite::v1::StreamingCommitCursorRequest,
      google::cloud::pubsublite::v1::StreamingCommitCursorResponse>;
  using ResumableStreamImpl = ResumableAsyncStreamingReadWriteRpcImpl<
      google::cloud::pubsublite::v1::StreamingCommitCursorRequest,
      google::cloud::pubsublite::v1::StreamingCommitCursorResponse>;

 public:
  explicit CommitSubscriber(
      absl::FunctionRef<std::unique_ptr<ResumableStream>(
          StreamInitializer<
              google::cloud::pubsublite::v1::StreamingCommitCursorRequest,
              google::cloud::pubsublite::v1::StreamingCommitCursorResponse>)>
          resumable_stream_factory,
      google::cloud::pubsublite::v1::InitialCommitCursorRequest
          initial_commit_request);

  ~CommitSubscriber() override;

  future<Status> Start() override;

  void Commit(google::cloud::pubsublite::v1::Cursor cursor);

  future<void> Shutdown() override;

 private:
  void SendCommits();

  void OnRead(absl::optional<
              google::cloud::pubsublite::v1::StreamingCommitCursorResponse>
                  response);

  void Read();

  future<StatusOr<ResumableAsyncStreamingReadWriteRpcImpl<
      google::cloud::pubsublite::v1::StreamingCommitCursorRequest,
      google::cloud::pubsublite::v1::StreamingCommitCursorResponse>::
                      UnderlyingStream>>
  Initializer(ResumableStreamImpl::UnderlyingStream stream);

  google::cloud::pubsublite::v1::InitialCommitCursorRequest const
      initial_commit_request_;

  std::mutex mu_;

  std::unique_ptr<ResumableStream> const
      resumable_stream_;  // ABSL_GUARDED_BY(mu_)
  ServiceComposite service_composite_;
  std::deque<google::cloud::pubsublite::v1::Cursor>
      outstanding_commits_;  // ABSL_GUARDED_BY(mu_)
  std::deque<google::cloud::pubsublite::v1::Cursor>
      to_be_sent_commits_;        // ABSL_GUARDED_BY(mu_)
  bool sending_commits_ = false;  // ABSL_GUARDED_BY(mu_)
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_COMMIT_SUBSCRIBER_H
