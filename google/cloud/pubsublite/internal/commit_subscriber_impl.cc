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

#include "google/cloud/pubsublite/internal/commit_subscriber_impl.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"

namespace google {
namespace cloud {
namespace pubsublite_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using google::cloud::pubsublite::v1::Cursor;
using google::cloud::pubsublite::v1::InitialCommitCursorRequest;
using google::cloud::pubsublite::v1::StreamingCommitCursorRequest;
using google::cloud::pubsublite::v1::StreamingCommitCursorResponse;

CommitSubscriberImpl::CommitSubscriberImpl(
    absl::FunctionRef<std::unique_ptr<ResumableStream>(
        StreamInitializer<
            google::cloud::pubsublite::v1::StreamingCommitCursorRequest,
            google::cloud::pubsublite::v1::StreamingCommitCursorResponse>)>
        resumable_stream_factory,
    google::cloud::pubsublite::v1::InitialCommitCursorRequest
        initial_commit_request)
    : initial_commit_request_{std::move(initial_commit_request)},
      resumable_stream_{resumable_stream_factory(std::bind(
          &CommitSubscriberImpl::Initializer, this, std::placeholders::_1))},
      service_composite_{resumable_stream_.get()} {}

future<Status> CommitSubscriberImpl::Start() {
  auto start_return = service_composite_.Start();
  Read();
  return start_return;
}

void CommitSubscriberImpl::Commit(Cursor cursor) {
  {
    std::lock_guard<std::mutex> g{mu_};
    if ((to_be_sent_commit_ &&
         cursor.offset() <= to_be_sent_commit_->offset()) ||
        (last_outstanding_commit_ &&
         cursor.offset() <= last_outstanding_commit_->offset())) {
      return service_composite_.Abort(Status(
          StatusCode::kFailedPrecondition,
          absl::StrCat("offset ", cursor.offset(),
                       " is less than or equal to previous sent offsets")));
    }
    to_be_sent_commit_ = std::move(cursor);
    if (sending_commits_) return;
    sending_commits_ = true;
  }
  SendCommits();
}

future<void> CommitSubscriberImpl::Shutdown() {
  return service_composite_.Shutdown();
}

void CommitSubscriberImpl::SendCommits() {
  StreamingCommitCursorRequest req;
  bool service_ok = service_composite_.status().ok();
  AsyncRoot root;
  std::lock_guard<std::mutex> g{mu_};
  if (!to_be_sent_commit_ || !service_ok) {
    sending_commits_ = false;
    return;
  }
  *req.mutable_commit()->mutable_cursor() = *to_be_sent_commit_;
  last_outstanding_commit_ = *std::move(to_be_sent_commit_);
  ++num_outstanding_commits_;
  to_be_sent_commit_.reset();

  root.get_future()
      .then(ChainFuture(resumable_stream_->Write(std::move(req))))
      .then([this](future<bool>) { SendCommits(); });
}

void CommitSubscriberImpl::OnRead(
    absl::optional<StreamingCommitCursorResponse> response) {
  // optional not engaged implies that the retry loop has finished
  if (!response) return Read();
  if (!response->has_commit()) {
    return service_composite_.Abort(Status(
        StatusCode::kInternal,
        absl::StrCat("Invalid `Read` response: ", response->DebugString())));
  }

  auto num_commits_acked =
      static_cast<std::uint64_t>(response->commit().acknowledged_commits());

  {
    std::lock_guard<std::mutex> g{mu_};
    if (num_commits_acked > num_outstanding_commits_) {
      return service_composite_.Abort(Status(
          StatusCode::kInternal,
          absl::StrCat(
              "Number commits acked: ", num_commits_acked,
              " > num outstanding commits: ", num_outstanding_commits_)));
    }
    num_outstanding_commits_ -= num_commits_acked;
  }
  Read();
}

void CommitSubscriberImpl::Read() {
  if (!service_composite_.status().ok()) return;
  AsyncRoot root;
  // need lock because calling `resumable_stream_->Read()`
  std::lock_guard<std::mutex> g{mu_};
  root.get_future()
      .then(ChainFuture(resumable_stream_->Read()))
      .then(
          [this](
              future<absl::optional<StreamingCommitCursorResponse>> response) {
            OnRead(response.get());
          });
}

future<StatusOr<ResumableAsyncStreamingReadWriteRpcImpl<
    StreamingCommitCursorRequest,
    StreamingCommitCursorResponse>::UnderlyingStream>>
CommitSubscriberImpl::Initializer(
    ResumableStreamImpl::UnderlyingStream stream) {
  // By the time initializer is called, no outstanding Read() or Write()
  // futures will be outstanding.
  auto shared_stream = std::make_shared<ResumableStreamImpl::UnderlyingStream>(
      std::move(stream));
  StreamingCommitCursorRequest commit_request;
  *commit_request.mutable_initial() = initial_commit_request_;
  return (*shared_stream)
      ->Write(commit_request, grpc::WriteOptions())
      .then([shared_stream](future<bool> write_response) {
        if (!write_response.get()) {
          return make_ready_future(
              absl::optional<StreamingCommitCursorResponse>());
        }
        return (*shared_stream)->Read();
      })
      .then([this, shared_stream](
                future<absl::optional<StreamingCommitCursorResponse>>
                    read_response) {
        auto response = read_response.get();
        if (!response || !response->has_initial()) {
          return make_ready_future(false);
        }
        StreamingCommitCursorRequest req;
        {
          std::lock_guard<std::mutex> g{mu_};
          // this can happen if we're initializing the first stream and haven't
          // called `Commit` yet
          if (!sending_commits_) return make_ready_future(true);
          // we can disregard all but last outstanding commits on last stream
          num_outstanding_commits_ = 1;
          *req.mutable_commit()->mutable_cursor() = *last_outstanding_commit_;
        }
        return (*shared_stream)->Write(std::move(req), grpc::WriteOptions());
      })
      .then([shared_stream](future<bool> res) {
        if (!res.get()) return (*shared_stream)->Finish();
        return make_ready_future(Status());
      })
      .then([shared_stream](future<Status> f)
                -> StatusOr<ResumableStreamImpl::UnderlyingStream> {
        Status status = f.get();
        if (!status.ok()) return status;
        return std::move(*shared_stream);
      });
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google
