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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_COMMITTER_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_COMMITTER_IMPL_H

#include "google/cloud/pubsublite/internal/committer.h"
#include "google/cloud/pubsublite/internal/resumable_async_streaming_read_write_rpc.h"
#include "google/cloud/pubsublite/internal/service_composite.h"
#include "absl/functional/function_ref.h"
#include <google/cloud/pubsublite/v1/cursor.pb.h>
#include <mutex>

namespace google {
namespace cloud {
namespace pubsublite_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class CommitterImpl : public Committer {
 private:
  using ResumableStream = ResumableAsyncStreamingReadWriteRpc<
      google::cloud::pubsublite::v1::StreamingCommitCursorRequest,
      google::cloud::pubsublite::v1::StreamingCommitCursorResponse>;
  using UnderlyingStream = std::unique_ptr<AsyncStreamingReadWriteRpc<
      google::cloud::pubsublite::v1::StreamingCommitCursorRequest,
      google::cloud::pubsublite::v1::StreamingCommitCursorResponse>>;

 public:
  explicit CommitterImpl(
      absl::FunctionRef<std::unique_ptr<ResumableStream>(
          StreamInitializer<
              google::cloud::pubsublite::v1::StreamingCommitCursorRequest,
              google::cloud::pubsublite::v1::StreamingCommitCursorResponse>)>
          resumable_stream_factory,
      google::cloud::pubsublite::v1::InitialCommitCursorRequest
          initial_commit_request);

  future<Status> Start() override;

  void Commit(google::cloud::pubsublite::v1::Cursor cursor) override;

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
  Initializer(UnderlyingStream stream);

  google::cloud::pubsublite::v1::InitialCommitCursorRequest const
      initial_commit_request_;
  std::unique_ptr<ResumableStream> const resumable_stream_;

  std::mutex mu_;

  absl::optional<google::cloud::pubsublite::v1::Cursor>
      last_sent_commit_;                      // ABSL_GUARDED_BY(mu_)
  std::int64_t num_outstanding_commits_ = 0;  // ABSL_GUARDED_BY(mu_)
  absl::optional<google::cloud::pubsublite::v1::Cursor>
      to_be_sent_commit_;         // ABSL_GUARDED_BY(mu_)
  bool sending_commits_ = false;  // ABSL_GUARDED_BY(mu_)
  ServiceComposite service_composite_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_COMMITTER_IMPL_H
