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

#include "google/cloud/pubsub/internal/sequential_batch_sink.h"
#include "google/cloud/internal/async_retry_loop.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

SequentialBatchSink::SequentialBatchSink(
    std::shared_ptr<pubsub_internal::BatchSink> sink)
    : sink_(std::move(sink)) {}

future<StatusOr<google::pubsub::v1::PublishResponse>>
SequentialBatchSink::AsyncPublish(google::pubsub::v1::PublishRequest request) {
  std::unique_lock<std::mutex> lk(mu_);
  if (!corked_on_error_.ok()) {
    return make_ready_future(StatusOr<PublishResponse>(corked_on_error_));
  }
  if (corked_on_pending_) {
    queue_.push_back({std::move(request), {}});
    return queue_.back().promise.get_future();
  }
  corked_on_pending_ = true;
  lk.unlock();

  auto weak = WeakFromThis();
  return sink_->AsyncPublish(std::move(request))
      .then([weak](future<StatusOr<PublishResponse>> f) {
        auto r = f.get();
        auto status = r ? Status{} : r.status();
        if (auto self = weak.lock()) self->OnPublish(std::move(status));
        return r;
      });
}

void SequentialBatchSink::ResumePublish(std::string const& ordering_key) {
  {
    std::lock_guard<std::mutex> lk(mu_);
    corked_on_error_ = {};
  }
  sink_->ResumePublish(ordering_key);
}

void SequentialBatchSink::OnPublish(Status s) {
  std::unique_lock<std::mutex> lk(mu_);
  corked_on_pending_ = false;
  corked_on_error_ = std::move(s);

  // If the last result is an error drain the queue with that status, note that
  // no new elements will be added to the queue until ResumePublish() is called
  // by the application, as AsyncPublish() rejects messages.
  if (!corked_on_error_.ok()) {
    std::deque<PendingRequest> queue;
    queue.swap(queue_);
    auto error = corked_on_error_;
    lk.unlock();
    for (auto& p : queue) {
      p.promise.set_value(error);
    }
    return;
  }

  // If necessary, schedule the next call.
  if (queue_.empty()) return;
  corked_on_pending_ = true;
  auto pr = std::move(queue_.front());
  queue_.pop_front();
  lk.unlock();

  auto weak = WeakFromThis();
  struct MoveCaptureRequest {
    std::weak_ptr<SequentialBatchSink> weak;
    google::cloud::promise<StatusOr<PublishResponse>> promise;
    void operator()(future<StatusOr<PublishResponse>> f) {
      auto response = f.get();
      auto status = response ? Status{} : response.status();
      promise.set_value(std::move(response));
      if (auto self = weak.lock()) self->OnPublish(std::move(status));
    }
  };
  sink_->AsyncPublish(std::move(pr.request))
      .then(MoveCaptureRequest{WeakFromThis(), std::move(pr.promise)});
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
