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

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

// A helper callable to handle a response, it is a bit large for a lambda, and
// we need move-capture anyways.
struct Batch {
  std::vector<promise<StatusOr<std::string>>> waiters;
  std::weak_ptr<BatchingPublisherConnection> weak;

  void operator()(future<StatusOr<google::pubsub::v1::PublishResponse>> f) {
    auto response = f.get();
    if (!response) {
      SatisfyAllWaiters(response.status());
      if (auto batcher = weak.lock()) batcher->HandleError(response.status());
      return;
    }
    if (static_cast<std::size_t>(response->message_ids_size()) !=
        waiters.size()) {
      SatisfyAllWaiters(
          Status(StatusCode::kUnknown, "mismatched message id count"));
      return;
    }
    int idx = 0;
    for (auto& w : waiters) {
      w.set_value(std::move(*response->mutable_message_ids(idx++)));
    }
  }

  void SatisfyAllWaiters(Status const& status) {
    for (auto& w : waiters) w.set_value(status);
  }
};

future<StatusOr<std::string>> BatchingPublisherConnection::Publish(
    PublishParams p) {
  auto const bytes = pubsub_internal::MessageSize(p.message);
  std::unique_lock<std::mutex> lk(mu_);
  do {
    if (!corked_on_status_.ok()) return CorkedError();
    // If empty we need to create the batch, even if it would be oversized,
    // otherwise the message may be dropped.
    if (waiters_.empty()) break;
    auto const has_bytes_capacity =
        current_bytes_ + bytes <= options_.maximum_batch_bytes();
    auto const has_messages_capacity =
        waiters_.size() < options_.maximum_batch_message_count();
    // If there is enough room just add the message below.
    if (has_bytes_capacity && has_messages_capacity) break;
    // We need to flush the existing batch, that will release the lock, and then
    // we try again.
    FlushImpl(std::move(lk));
    lk = std::unique_lock<std::mutex>(mu_);
  } while (true);

  auto proto = pubsub_internal::ToProto(std::move(p.message));
  waiters_.emplace_back();
  auto f = waiters_.back().get_future();

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
  {
    std::unique_lock<std::mutex> lk(mu_);
    corked_on_status_ = {};
  }
  sink_->ResumePublish(p.ordering_key);
}

void BatchingPublisherConnection::HandleError(Status const& status) {
  std::unique_lock<std::mutex> lk(mu_);
  // An error should discard pending messages and block future messages only
  // if ordering is required.
  if (!RequiresOrdering()) return;
  corked_on_status_ = status;
  pending_.Clear();
  std::vector<promise<StatusOr<std::string>>> waiters;
  waiters.swap(waiters_);
  lk.unlock();
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
  auto const too_many_messages =
      waiters_.size() >= options_.maximum_batch_message_count();
  auto const too_many_bytes = current_bytes_ >= options_.maximum_batch_bytes();
  if (too_many_messages || too_many_bytes) {
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

future<StatusOr<std::string>> BatchingPublisherConnection::CorkedError() {
  promise<StatusOr<std::string>> p;
  auto f = p.get_future();

  struct MoveCapture {
    promise<StatusOr<std::string>> p;
    Status status;
    void operator()() { p.set_value(std::move(status)); }
  };
  cq_.RunAsync(MoveCapture{std::move(p), corked_on_status_});

  return f;
}

void BatchingPublisherConnection::FlushImpl(std::unique_lock<std::mutex> lk) {
  if (pending_.messages().empty()) return;

  Batch batch;
  batch.waiters.swap(waiters_);
  google::pubsub::v1::PublishRequest request;
  request.Swap(&pending_);
  // Reserve enough capacity for the next batch.
  pending_.mutable_messages()->Reserve(
      static_cast<int>(options_.maximum_batch_message_count()));
  current_bytes_ = 0;
  lk.unlock();

  batch.weak = shared_from_this();
  request.set_topic(topic_full_name_);
  sink_->AsyncPublish(std::move(request)).then(std::move(batch));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
