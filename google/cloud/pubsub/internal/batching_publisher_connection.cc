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

#include "google/cloud/pubsub/internal/batching_publisher_connection.h"
#include "google/cloud/internal/async_retry_loop.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

// A helper callable to handle a response, it is a bit large for a lambda, and
// we need move-capture anyways.
struct Batch {
  // We are going to use the completion queue as an executor, so we might as
  // well name it as one.
  google::cloud::CompletionQueue executor;
  std::vector<promise<StatusOr<std::string>>> waiters;
  std::weak_ptr<BatchingPublisherConnection> weak;

  void operator()(future<StatusOr<google::pubsub::v1::PublishResponse>> f) {
    auto response = f.get();
    if (!response) {
      SatisfyAllWaiters(response.status());
      if (auto batcher = weak.lock()) batcher->DiscardCorked(response.status());
      return;
    }
    if (static_cast<std::size_t>(response->message_ids_size()) !=
        waiters.size()) {
      SatisfyAllWaiters(
          Status(StatusCode::kUnknown, "mismatched message id count"));
      return;
    }
    struct SetValue {
      promise<StatusOr<std::string>> waiter;
      std::string value;
      void operator()() { waiter.set_value(std::move(value)); }
    };
    int idx = 0;
    for (auto& w : waiters) {
      executor.RunAsync(SetValue{std::move(w), response->message_ids(idx++)});
    }
    if (auto batcher = weak.lock()) batcher->UnCork();
  }

  void SatisfyAllWaiters(Status const& status) {
    struct SetStatus {
      promise<StatusOr<std::string>> waiter;
      Status status;
      void operator()() { waiter.set_value(std::move(status)); }
    };
    for (auto& w : waiters) {
      executor.RunAsync(SetStatus{std::move(w), status});
    }
  }
};

future<StatusOr<std::string>> BatchingPublisherConnection::Publish(
    PublishParams p) {
  promise<StatusOr<std::string>> pr;
  auto f = pr.get_future();
  auto const bytes = pubsub_internal::MessageSize(p.message);
  std::unique_lock<std::mutex> lk(mu_);
  if (!corked_status_.ok()) {
    struct MoveCapture {
      promise<StatusOr<std::string>> p;
      Status status;
      void operator()() { p.set_value(std::move(status)); }
    };
    cq_.RunAsync(MoveCapture{std::move(pr), corked_status_});
    return f;
  }
  auto proto = pubsub_internal::ToProto(std::move(p.message));
  waiters_.push_back(std::move(pr));

  // Use RAII to preserve the strong exception guarantee.
  struct UndoPush {
    std::vector<promise<StatusOr<std::string>>>* waiters;

    ~UndoPush() {
      if (waiters != nullptr) waiters->pop_back();
    }
    void release() { waiters = nullptr; }
  } undo{&waiters_};

  *pending_.add_messages() = std::move(proto);
  undo.release();  // no throws after this point, we can rest easy
  current_bytes_ += bytes;
  MaybeFlush(std::move(lk));
  return f;
}

void BatchingPublisherConnection::Flush(FlushParams) {
  FlushImpl(std::unique_lock<std::mutex>(mu_));
}

void BatchingPublisherConnection::ResumePublish(ResumePublishParams p) {
  if (ordering_key_ != p.ordering_key) return;
  UnCork();
}

void BatchingPublisherConnection::UnCork() {
  std::unique_lock<std::mutex> lk(mu_);
  corked_ = false;
  corked_status_ = {};
  MaybeFlush(std::move(lk));
}

void BatchingPublisherConnection::DiscardCorked(Status const& status) {
  auto waiters = [&] {
    std::unique_lock<std::mutex> lk(mu_);
    corked_ = true;
    corked_status_ = status;
    pending_.Clear();
    std::vector<promise<StatusOr<std::string>>> tmp;
    tmp.swap(waiters_);
    return tmp;
  }();
  for (auto& p : waiters) {
    struct MoveCapture {
      promise<StatusOr<std::string>> p;
      Status status;
      void operator()() { p.set_value(std::move(status)); }
    };
    cq_.RunAsync(MoveCapture{std::move(p), status});
  }
}

void BatchingPublisherConnection::MaybeFlush(std::unique_lock<std::mutex> lk) {
  if (pending_.messages_size() >=
      static_cast<std::int32_t>(options_.maximum_batch_message_count())) {
    FlushImpl(std::move(lk));
    return;
  }
  if (current_bytes_ >= options_.maximum_batch_bytes()) {
    FlushImpl(std::move(lk));
    return;
  }
  // If the batch is empty obviously we do not need a timer, and if it has more
  // than one element then we have setup a timer previously and there is no need
  // to set it again.
  if (pending_.messages_size() != 1) return;
  auto const expiration = batch_expiration_ =
      std::chrono::system_clock::now() + options_.maximum_hold_time();
  lk.unlock();
  // We need a weak_ptr<> because this class owns the completion queue,
  // creating a lambda with a shared_ptr<> owning this class would create a
  // cycle.  Unfortunately some older compiler/libraries lack
  // `weak_from_this()`.
  auto weak = std::weak_ptr<BatchingPublisherConnection>(shared_from_this());
  // Note that at this point the lock is released, so whether the timer
  // schedules later on schedules in this thread has no effect.
  cq_.MakeDeadlineTimer(expiration)
      .then([weak](future<StatusOr<std::chrono::system_clock::time_point>>) {
        if (auto self = weak.lock()) self->OnTimer();
      });
}

void BatchingPublisherConnection::OnTimer() {
  std::unique_lock<std::mutex> lk(mu_);
  if (std::chrono::system_clock::now() < batch_expiration_) {
    // We may get many "old" timers for batches that have already flushed due to
    // size. Trying to cancel these timers is a bit hopeless, they might trigger
    // even if we attempt to cancel them. This test is more robust.
    return;
  }
  FlushImpl(std::move(lk));
}

void BatchingPublisherConnection::FlushImpl(std::unique_lock<std::mutex> lk) {
  if (pending_.messages().empty() || corked_) return;

  Batch batch;
  batch.waiters.swap(waiters_);
  google::pubsub::v1::PublishRequest request;
  request.Swap(&pending_);
  corked_ = !ordering_key_.empty();
  current_bytes_ = 0;
  lk.unlock();

  auto context = absl::make_unique<grpc::ClientContext>();

  batch.executor = cq_;
  batch.weak = shared_from_this();
  request.set_topic(topic_full_name_);

  auto& stub = stub_;
  google::cloud::internal::AsyncRetryLoop(
      retry_policy_->clone(), backoff_policy_->clone(),
      google::cloud::internal::Idempotency::kIdempotent, cq_,
      [stub](google::cloud::CompletionQueue& cq,
             std::unique_ptr<grpc::ClientContext> context,
             google::pubsub::v1::PublishRequest const& request) {
        return stub->AsyncPublish(cq, std::move(context), request);
      },
      std::move(request), __func__)
      .then(std::move(batch));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
