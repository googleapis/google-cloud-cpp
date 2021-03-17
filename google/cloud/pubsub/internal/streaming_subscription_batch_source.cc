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

#include "google/cloud/pubsub/internal/streaming_subscription_batch_source.h"
#include "google/cloud/log.h"
#include <ostream>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

void StreamingSubscriptionBatchSource::Start(BatchCallback callback) {
  std::unique_lock<std::mutex> lk(mu_);
  if (callback_) return;
  callback_ = std::move(callback);
  lk.unlock();

  StartWriteTimer();
  shutdown_manager_->StartOperation(__func__, "stream", [this] {
    StartStream(retry_policy_->clone(), backoff_policy_->clone());
  });
}

void StreamingSubscriptionBatchSource::Shutdown() {
  std::unique_lock<std::mutex> lk(mu_);
  if (shutdown_ || !stream_) return;
  shutdown_ = true;
  if (stream_) stream_->Cancel();
}

void StreamingSubscriptionBatchSource::AckMessage(std::string const& ack_id) {
  std::unique_lock<std::mutex> lk(mu_);
  ack_queue_.push_back(ack_id);
  DrainQueues(std::move(lk), false);
}

void StreamingSubscriptionBatchSource::NackMessage(std::string const& ack_id) {
  std::unique_lock<std::mutex> lk(mu_);
  nack_queue_.push_back(ack_id);
  DrainQueues(std::move(lk), false);
}

void StreamingSubscriptionBatchSource::BulkNack(
    std::vector<std::string> ack_ids) {
  std::unique_lock<std::mutex> lk(mu_);
  for (auto& a : ack_ids) {
    nack_queue_.push_back(std::move(a));
  }
  DrainQueues(std::move(lk), false);
}

void StreamingSubscriptionBatchSource::ExtendLeases(
    std::vector<std::string> ack_ids, std::chrono::seconds extension) {
  std::unique_lock<std::mutex> lk(mu_);
  for (auto& a : ack_ids) {
    deadlines_queue_.emplace_back(std::move(a), extension);
  }
  DrainQueues(std::move(lk), false);
}

void StreamingSubscriptionBatchSource::StartStream(
    std::shared_ptr<pubsub::RetryPolicy> retry_policy,
    std::shared_ptr<pubsub::BackoffPolicy> backoff_policy) {
  // Starting a stream is a 4 step process.
  // 1. Create a new SubscriberStub::AsyncPullStream object.
  // 2. Call Start() on it, which is asynchronous, which might fail, but rarely
  //    does
  // 3. Call Write() on it, which is also asynchronous and almost always
  //    succeeds, could fail if the endpoint is invalid
  // 4. Call Read() on it, which is also asynchronous, and it is the first
  //    chance to get errors such as kPermissionDenied, or kNotFound.
  // Once Read() completes we can repeatedly call Read() and/or Write().
  // Because steps 2 through 4 may fail with transient errors we need to wrap
  // these steps in an asynchronous retry loop.

  auto request = InitialRequest();
  auto stream = stub_->AsyncStreamingPull(
      cq_, absl::make_unique<grpc::ClientContext>(), request);
  if (!stream) {
    OnRetryFailure(Status(StatusCode::kUnknown, "null stream"));
    return;
  }
  shutdown_manager_->StartOperation(__func__, "InitialStart", [&] {
    stream_ = std::move(stream);
    RetryLoopState rs{std::move(retry_policy), std::move(backoff_policy)};
    auto weak = WeakFromThis();
    stream_->Start().then([weak, rs, request](future<bool> f) {
      if (auto s = weak.lock()) s->OnStart(rs, request, f.get());
    });
  });
}

google::pubsub::v1::StreamingPullRequest
StreamingSubscriptionBatchSource::InitialRequest() const {
  google::pubsub::v1::StreamingPullRequest request;
  request.set_subscription(subscription_full_name_);
  request.set_client_id(client_id_);
  request.set_max_outstanding_bytes(max_outstanding_bytes_);
  request.set_max_outstanding_messages(max_outstanding_messages_);
  auto const deadline =
      std::max(std::chrono::seconds(10),
               std::min(std::chrono::seconds(600), max_deadline_time_))
          .count();
  request.set_stream_ack_deadline_seconds(static_cast<std::int32_t>(deadline));
  return request;
}

void StreamingSubscriptionBatchSource::OnStart(
    RetryLoopState rs, google::pubsub::v1::StreamingPullRequest const& request,
    bool ok) {
  shutdown_manager_->FinishedOperation("InitialStart");
  if (!ok) {
    OnInitialError(std::move(rs));
    return;
  }
  shutdown_manager_->StartOperation(__func__, "InitialWrite", [&] {
    auto weak = WeakFromThis();
    stream_->Write(request, grpc::WriteOptions{}.set_write_through())
        .then([weak, rs](future<bool> f) mutable {
          if (auto s = weak.lock()) s->OnInitialWrite(std::move(rs), f.get());
        });
  });
}

void StreamingSubscriptionBatchSource::OnInitialWrite(RetryLoopState const& rs,
                                                      bool ok) {
  shutdown_manager_->FinishedOperation("InitialWrite");
  if (!ok) {
    OnInitialError(std::move(rs));
    return;
  }
  auto scheduled =
      shutdown_manager_->StartOperation(__func__, "InitialRead", [&] {
        auto weak = WeakFromThis();
        stream_->Read().then(
            [weak,
             rs](future<
                 absl::optional<google::pubsub::v1::StreamingPullResponse>>
                     f) {
              if (auto s = weak.lock())
                s->OnInitialRead(std::move(rs), f.get());
            });
      });
  // This is very rare, but it can happen if the session enters shutdown while
  // the initial setup is in progress.
  if (!scheduled) OnInitialError(std::move(rs));
}

void StreamingSubscriptionBatchSource::OnInitialRead(
    RetryLoopState rs,
    absl::optional<google::pubsub::v1::StreamingPullResponse> response) {
  shutdown_manager_->FinishedOperation("InitialRead");
  if (!response.has_value()) {
    OnInitialError(std::move(rs));
    return;
  }

  std::unique_lock<std::mutex> lk(mu_);
  ChangeState(lk, StreamState::kActive, __func__, "success");
  status_ = Status{};
  lk.unlock();
  auto const scheduled =
      shutdown_manager_->StartOperation(__func__, "read", [&] {
        OnRead(std::move(response));
        shutdown_manager_->FinishedOperation("read");
      });
  if (!scheduled) {
    Shutdown();
    lk.lock();
    ShutdownStream(std::move(lk), "early-shutdown");
  }
}

void StreamingSubscriptionBatchSource::OnInitialError(RetryLoopState rs) {
  auto weak = WeakFromThis();
  auto const scheduled =
      shutdown_manager_->StartOperation(__func__, "finish", [&] {
        stream_->Finish().then([weak, rs](future<Status> f) {
          if (auto s = weak.lock()) s->OnInitialFinish(std::move(rs), f.get());
        });
        shutdown_manager_->FinishedOperation("finish");
      });
  if (!scheduled) {
    shutdown_manager_->FinishedOperation("stream");
  }
}

void StreamingSubscriptionBatchSource::OnInitialFinish(RetryLoopState rs,
                                                       Status status) {
  if (!rs.retry_policy->OnFailure(status)) {
    OnRetryFailure(std::move(status));
    return;
  }
  auto weak = WeakFromThis();
  using F = future<StatusOr<std::chrono::system_clock::time_point>>;
  cq_.MakeRelativeTimer(rs.backoff_policy->OnCompletion())
      .then([weak, rs, status](F f) mutable {
        auto s = weak.lock();
        if (!s) return;
        if (f.get().ok()) {
          s->OnBackoff(std::move(rs), std::move(status));
        } else {
          s->shutdown_manager_->FinishedOperation("stream");
        }
      });
}

void StreamingSubscriptionBatchSource::OnBackoff(RetryLoopState rs,
                                                 Status status) {
  if (rs.retry_policy->IsExhausted()) {
    OnRetryFailure(std::move(status));
    return;
  }
  auto const scheduled =
      shutdown_manager_->StartOperation(__func__, "retry", [&] {
        StartStream(std::move(rs.retry_policy), std::move(rs.backoff_policy));
        shutdown_manager_->FinishedOperation("retry");
      });
  if (!scheduled) {
    shutdown_manager_->FinishedOperation("stream");
  }
}

void StreamingSubscriptionBatchSource::OnRetryFailure(Status status) {
  if (shutdown_manager_->FinishedOperation("stream")) return;
  shutdown_manager_->MarkAsShutdown(__func__, status);
  callback_(std::move(status));
}

void StreamingSubscriptionBatchSource::ReadLoop() {
  std::unique_lock<std::mutex> lk(mu_);
  if (stream_state_ != StreamState::kActive) return;
  pending_read_ = true;
  auto stream = stream_;
  lk.unlock();
  auto weak = WeakFromThis();
  using ResponseType =
      absl::optional<google::pubsub::v1::StreamingPullResponse>;
  stream->Read().then([weak, stream](future<ResponseType> f) {
    if (auto self = weak.lock()) self->OnRead(f.get());
  });
}

void StreamingSubscriptionBatchSource::OnRead(
    absl::optional<google::pubsub::v1::StreamingPullResponse> response) {
  auto weak = WeakFromThis();
  std::unique_lock<std::mutex> lk(mu_);
  pending_read_ = false;
  if (response && stream_state_ == StreamState::kActive && !shutdown_) {
    lk.unlock();
    callback_(*std::move(response));
    cq_.RunAsync([weak] {
      if (auto self = weak.lock()) self->ReadLoop();
    });
    return;
  }
  ShutdownStream(std::move(lk), response ? "state" : "read error");
}

void StreamingSubscriptionBatchSource::ShutdownStream(
    std::unique_lock<std::mutex> lk, char const* reason) {
  if (stream_state_ != StreamState::kActive &&
      stream_state_ != StreamState::kDisconnecting) {
    return;
  }
  ChangeState(lk, StreamState::kDisconnecting, __func__, reason);
  if (pending_read_ || pending_write_) return;

  auto stream = stream_;
  ChangeState(lk, StreamState::kFinishing, __func__, reason);
  lk.unlock();
  auto weak = WeakFromThis();
  // There are no pending reads or writes, and something (probable a read or
  // write error) recommends we shutdown the stream
  stream->Finish().then([weak, stream](future<Status> f) {
    if (auto self = weak.lock()) self->OnFinish(f.get());
  });
}

void StreamingSubscriptionBatchSource::OnFinish(Status status) {
  std::unique_lock<std::mutex> lk(mu_);
  status_ = std::move(status);
  stream_.reset();
  ChangeState(lk, StreamState::kNull, __func__, "done");
  if (shutdown_manager_->FinishedOperation("stream")) return;
  lk.unlock();
  shutdown_manager_->StartOperation(__func__, "stream", [this] {
    StartStream(retry_policy_->clone(), backoff_policy_->clone());
  });
}

void StreamingSubscriptionBatchSource::DrainQueues(
    std::unique_lock<std::mutex> lk, bool force_flush) {
  auto const threshold = force_flush ? 1 : ack_batching_config_.max_batch_size;
  if (ack_queue_.size() < threshold && nack_queue_.size() < threshold &&
      deadlines_queue_.empty()) {
    return;
  }
  if (stream_state_ != StreamState::kActive || pending_write_) return;
  auto stream = stream_;
  pending_write_ = true;

  std::vector<std::string> acks;
  acks.swap(ack_queue_);
  std::vector<std::string> nacks;
  nacks.swap(nack_queue_);
  std::vector<std::pair<std::string, std::chrono::seconds>> deadlines;
  deadlines.swap(deadlines_queue_);
  lk.unlock();

  google::pubsub::v1::StreamingPullRequest request;
  for (auto& a : acks) request.add_ack_ids(std::move(a));
  for (auto& n : nacks) {
    request.add_modify_deadline_ack_ids(std::move(n));
    request.add_modify_deadline_seconds(0);
  }
  for (auto& d : deadlines) {
    request.add_modify_deadline_ack_ids(std::move(d.first));
    request.add_modify_deadline_seconds(
        static_cast<std::int32_t>(d.second.count()));
  }
  // Note that we do not use `AsyncRetryLoop()` here. The ack/nack pipeline is
  // best-effort anyway, there is no guarantee that the server will act on any
  // of these.
  auto weak = WeakFromThis();
  stream->Write(request, grpc::WriteOptions{}.set_write_through())
      .then([weak, stream](future<bool> f) {
        if (auto self = weak.lock()) self->OnWrite(f.get());
      });
}

void StreamingSubscriptionBatchSource::OnWrite(bool ok) {
  std::unique_lock<std::mutex> lk(mu_);
  pending_write_ = false;
  if (ok && stream_state_ == StreamState::kActive && !shutdown_) {
    DrainQueues(std::move(lk), false);
    return;
  }
  ShutdownStream(std::move(lk), ok ? "state" : "write error");
}

void StreamingSubscriptionBatchSource::StartWriteTimer() {
  auto weak = WeakFromThis();
  using F = future<StatusOr<std::chrono::system_clock::time_point>>;
  cq_.MakeRelativeTimer(ack_batching_config_.max_hold_time).then([weak](F f) {
    if (auto self = weak.lock()) self->OnWriteTimer(f.get().status());
  });
}

void StreamingSubscriptionBatchSource::OnWriteTimer(Status const& s) {
  if (!s.ok()) return;
  std::unique_lock<std::mutex> lk(mu_);
  DrainQueues(std::move(lk), true);
  StartWriteTimer();
}

std::ostream& operator<<(std::ostream& os,
                         StreamingSubscriptionBatchSource::StreamState s) {
  using State = StreamingSubscriptionBatchSource::StreamState;
  switch (s) {
    case State::kNull:
      os << "kNull";
      break;
    case State::kActive:
      os << "kActive";
      break;
    case State::kDisconnecting:
      os << "kDisconnecting";
      break;
    case State::kFinishing:
      os << "kFinishing";
      break;
  }
  return os;
}

void StreamingSubscriptionBatchSource::ChangeState(
    std::unique_lock<std::mutex> const&, StreamState s, char const* where,
    char const* reason) {
  GCP_LOG(TRACE) << where << " (" << reason << ") " << stream_state_ << ":" << s
                 << " read=" << pending_read_ << " write=" << pending_write_
                 << " shutdown=" << shutdown_
                 << " stream=" << (stream_ ? "not-null" : "null")
                 << " status=" << status_;
  stream_state_ = s;
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
