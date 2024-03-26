// Copyright 2020 Google LLC
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

#include "google/cloud/pubsub/internal/streaming_subscription_batch_source.h"
#include "google/cloud/pubsub/internal/exactly_once_policies.h"
#include "google/cloud/pubsub/internal/extend_leases_with_retry.h"
#include "google/cloud/internal/async_retry_loop.h"
#include "google/cloud/internal/url_encode.h"
#include "google/cloud/log.h"
#include <iterator>
#include <ostream>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {
// NOLINTNEXTLINE(misc-no-recursion)
future<std::vector<Status>> WaitAll(std::vector<future<Status>> v) {
  if (v.empty()) return make_ready_future(std::vector<Status>{});
  auto back = std::move(v.back());
  v.pop_back();
  return WaitAll(std::move(v)).then([b = std::move(back)](auto f) mutable {
    return b.then([list = f.get()](auto g) mutable {
      list.push_back(g.get());
      return list;
    });
  });
}

future<Status> Reduce(std::vector<future<Status>> v) {
  return WaitAll(std::move(v)).then([](auto f) {
    auto ready = f.get();
    for (auto& s : ready) {
      if (!s.ok()) return std::move(s);
    }
    return Status{};
  });
}

}  // namespace

StreamingSubscriptionBatchSource::StreamingSubscriptionBatchSource(
    CompletionQueue cq,
    std::shared_ptr<SessionShutdownManager> shutdown_manager,
    std::shared_ptr<SubscriberStub> stub, std::string subscription_full_name,
    std::string client_id, Options opts)
    : cq_(std::move(cq)),
      shutdown_manager_(std::move(shutdown_manager)),
      stub_(std::move(stub)),
      subscription_full_name_(std::move(subscription_full_name)),
      client_id_(std::move(client_id)),
      options_(google::cloud::internal::MakeImmutableOptions(std::move(opts))),
      max_outstanding_messages_(
          options_->get<pubsub::MaxOutstandingMessagesOption>()),
      max_outstanding_bytes_(
          options_->get<pubsub::MaxOutstandingBytesOption>()),
      min_deadline_time_(options_->get<pubsub::MinDeadlineExtensionOption>()),
      max_deadline_time_(options_->get<pubsub::MaxDeadlineTimeOption>()) {}

void StreamingSubscriptionBatchSource::Start(
    std::shared_ptr<BatchCallback> callback) {
  std::unique_lock<std::mutex> lk(mu_);
  if (callback_) return;
  callback_ = std::move(callback);
  lk.unlock();

  shutdown_manager_->StartOperation(__func__, "stream", [this] {
    StartStream(options_->get<pubsub::RetryPolicyOption>()->clone(),
                options_->get<pubsub::BackoffPolicyOption>()->clone());
  });
}

void StreamingSubscriptionBatchSource::Shutdown() {
  internal::OptionsSpan span(options_);

  std::unique_lock<std::mutex> lk(mu_);
  if (shutdown_ || !stream_) return;
  shutdown_ = true;
  if (stream_) stream_->Cancel();
}

future<Status> StreamingSubscriptionBatchSource::AckMessage(
    std::string const& ack_id) {
  callback_->AddEvent(ack_id, "gl-cpp.ack_start");
  google::pubsub::v1::AcknowledgeRequest request;
  request.set_subscription(subscription_full_name_);
  *request.add_ack_ids() = ack_id;

  std::unique_lock<std::mutex> lk(mu_);
  if (exactly_once_delivery_enabled_.value_or(false)) {
    lk.unlock();
    auto retry = std::make_unique<ExactlyOnceRetryPolicy>(ack_id);
    return google::cloud::internal::AsyncRetryLoop(
        std::move(retry), ExactlyOnceBackoffPolicy(), Idempotency::kIdempotent,
        cq_,
        [stub = stub_, cb = callback_, ack_id](
            auto& cq, auto context, auto options, auto const& request) {
          return stub
              ->AsyncAcknowledge(cq, std::move(context), std::move(options),
                                 request)
              .then([cb, ack_id](auto f) {
                auto result = f.get();
                cb->EndMessage(ack_id, "gl-cpp.ack_end");
                return result;
              });
        },
        options_, std::move(request), __func__);
  }
  lk.unlock();
  return stub_
      ->AsyncAcknowledge(cq_, std::make_shared<grpc::ClientContext>(), options_,
                         request)
      .then([cb = callback_, ack_id](auto f) {
        auto result = f.get();
        cb->EndMessage(ack_id, "gl-cpp.ack_end");
        return result;
      });
}

future<Status> StreamingSubscriptionBatchSource::NackMessage(
    std::string const& ack_id) {
  callback_->AddEvent(ack_id, "gl-cpp.nack_start");
  google::pubsub::v1::ModifyAckDeadlineRequest request;
  request.set_subscription(subscription_full_name_);
  *request.add_ack_ids() = ack_id;
  request.set_ack_deadline_seconds(0);

  std::unique_lock<std::mutex> lk(mu_);
  if (exactly_once_delivery_enabled_.value_or(false)) {
    lk.unlock();
    auto retry = std::make_unique<ExactlyOnceRetryPolicy>(ack_id);
    return google::cloud::internal::AsyncRetryLoop(
        std::move(retry), ExactlyOnceBackoffPolicy(), Idempotency::kIdempotent,
        cq_,
        [stub = stub_, callback = callback_, ack_id](
            auto& cq, auto context, auto options, auto const& request) {
          return stub
              ->AsyncModifyAckDeadline(cq, std::move(context),
                                       std::move(options), request)
              .then([cb = callback, ack_id](auto f) {
                auto result = f.get();
                cb->EndMessage(ack_id, "gl-cpp.nack_end");
                return result;
              });
        },
        options_, std::move(request), __func__);
  }
  lk.unlock();
  return stub_
      ->AsyncModifyAckDeadline(cq_, std::make_shared<grpc::ClientContext>(),
                               options_, request)
      .then([cb = callback_, ack_id](auto f) {
        auto result = f.get();
        cb->EndMessage(ack_id, "gl-cpp.nack_end");
        return result;
      });
}

future<Status> StreamingSubscriptionBatchSource::BulkNack(
    std::vector<std::string> ack_ids) {
  google::pubsub::v1::ModifyAckDeadlineRequest request;
  request.set_subscription(subscription_full_name_);
  for (auto& a : ack_ids) *request.add_ack_ids() = std::move(a);
  request.set_ack_deadline_seconds(0);

  auto requests =
      SplitModifyAckDeadline(std::move(request), kMaxAckIdsPerMessage);
  if (requests.size() == 1) {
    std::string const ack_id = *requests.front().ack_ids().begin();
    callback_->AddEvent(ack_id, "gl-cpp.nack_start");
    return stub_
        ->AsyncModifyAckDeadline(cq_, std::make_shared<grpc::ClientContext>(),
                                 options_, requests.front())
        .then([cb = callback_, ack_id](auto f) {
          auto result = f.get();
          cb->EndMessage(ack_id, "gl-cpp.nack_end");
          return result;
        });
  }

  std::vector<future<Status>> pending(requests.size());
  std::transform(requests.begin(), requests.end(), pending.begin(),
                 [this](auto const& request) {
                   std::string const ack_id = *request.ack_ids().begin();
                   callback_->AddEvent(ack_id, "gl-cpp.nack_start");
                   return stub_
                       ->AsyncModifyAckDeadline(
                           cq_, std::make_shared<grpc::ClientContext>(),
                           options_, request)
                       .then([cb = callback_, ack_id](auto f) {
                         auto result = f.get();
                         cb->EndMessage(ack_id, "gl-cpp.nack_end");
                         return result;
                       });
                 });
  return Reduce(std::move(pending));
}

void StreamingSubscriptionBatchSource::ExtendLeases(
    std::vector<std::string> ack_ids, std::chrono::seconds extension) {
  google::pubsub::v1::ModifyAckDeadlineRequest request;
  request.set_subscription(subscription_full_name_);
  request.set_ack_deadline_seconds(
      static_cast<std::int32_t>(extension.count()));
  for (auto& a : ack_ids) {
    callback_->AddEvent(a, "gl-cpp.modack_start");
    request.add_ack_ids(std::move(a));
  }
  std::unique_lock<std::mutex> lk(mu_);
  auto split = SplitModifyAckDeadline(std::move(request), kMaxAckIdsPerMessage);
  if (exactly_once_delivery_enabled_.value_or(false)) {
    lk.unlock();
    for (auto& r : split) (void)ExtendLeasesWithRetry(stub_, cq_, std::move(r));
    return;
  }
  lk.unlock();
  for (auto& r : split) {
    (void)stub_
        ->AsyncModifyAckDeadline(cq_, std::make_shared<grpc::ClientContext>(),
                                 options_, r)
        .then([cb = callback_, r](auto f) {
          auto result = f.get();
          for (auto ack_id : r.ack_ids()) {
            cb->AddEvent(ack_id, "gl-cpp.modack_end");
          }
          return result;
        });
    ;
  }
}

void StreamingSubscriptionBatchSource::StartStream(
    std::shared_ptr<pubsub::RetryPolicy> retry_policy,
    std::shared_ptr<pubsub::BackoffPolicy> backoff_policy) {
  internal::OptionsSpan span(*options_);

  // Starting a stream is a 4-step process.
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
  auto context = std::make_shared<grpc::ClientContext>();
  context->AddMetadata(
      "x-goog-request-params",
      "subscription=" + internal::UrlEncode(request.subscription()));
  auto stream = stub_->AsyncStreamingPull(cq_, std::move(context), options_);
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
  if (!ok) return OnInitialError(rs);
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
  if (!scheduled) OnInitialError(rs);
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
  callback_->callback(BatchCallback::StreamingPullResponse{std::move(status)});
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
    auto update_stream_deadline = false;
    if (response->has_subscription_properties()) {
      auto const enabled =
          response->subscription_properties().exactly_once_delivery_enabled();
      if (exactly_once_delivery_enabled_ != enabled) {
        exactly_once_delivery_enabled_ = enabled;
        update_stream_deadline = true;
      }
    }
    lk.unlock();
    callback_->callback(
        BatchCallback::StreamingPullResponse{*std::move(response)});
    cq_.RunAsync([weak, update_stream_deadline] {
      auto self = weak.lock();
      if (!self) return;
      if (update_stream_deadline) self->UpdateStreamDeadline();
      self->ReadLoop();
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
  // write error) recommends we shut down the stream
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
    StartStream(options_->get<pubsub::RetryPolicyOption>()->clone(),
                options_->get<pubsub::BackoffPolicyOption>()->clone());
  });
}

void StreamingSubscriptionBatchSource::UpdateStreamDeadline() {
  std::unique_lock<std::mutex> lk(mu_);
  if (stream_state_ != StreamState::kActive || pending_write_) return;
  auto stream = stream_;
  pending_write_ = true;

  auto stream_ack_deadline = std::chrono::seconds(0);
  if (exactly_once_delivery_enabled_.value_or(false)) {
    stream_ack_deadline = min_deadline_time_;
  }
  lk.unlock();

  google::pubsub::v1::StreamingPullRequest request;
  request.set_stream_ack_deadline_seconds(
      static_cast<std::int32_t>(stream_ack_deadline.count()));
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
    return;
  }
  ShutdownStream(std::move(lk), ok ? "state" : "write error");
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

std::vector<google::pubsub::v1::ModifyAckDeadlineRequest>
SplitModifyAckDeadline(google::pubsub::v1::ModifyAckDeadlineRequest request,
                       int max_ack_ids) {
  // We expect this to be the common case.
  if (request.ack_ids_size() <= max_ack_ids) return {std::move(request)};

  std::vector<google::pubsub::v1::ModifyAckDeadlineRequest> result;
  auto& source = *request.mutable_ack_ids();
  while (request.ack_ids_size() > max_ack_ids) {
    google::pubsub::v1::ModifyAckDeadlineRequest r;
    r.set_subscription(request.subscription());
    r.set_ack_deadline_seconds(request.ack_deadline_seconds());

    auto begin = source.begin();
    auto end = std::next(source.begin(), max_ack_ids);
    r.mutable_ack_ids()->Reserve(max_ack_ids);
    for (auto i = begin; i != end; ++i) r.add_ack_ids(std::move(*i));
    source.erase(begin, end);
    result.push_back(std::move(r));
  }
  if (!request.ack_ids().empty()) result.push_back(std::move(request));
  return result;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
