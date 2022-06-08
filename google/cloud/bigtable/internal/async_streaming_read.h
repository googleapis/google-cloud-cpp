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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_STREAMING_READ_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_STREAMING_READ_H

#include "google/cloud/internal/async_streaming_read_rpc.h"
#include "absl/types/variant.h"
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

template <typename Response, typename OnReadHandler, typename OnFinishHandler>
class AsyncStreamingReadImpl
    : public std::enable_shared_from_this<
          AsyncStreamingReadImpl<Response, OnReadHandler, OnFinishHandler>> {
  static_assert(
      google::cloud::internal::is_invocable<OnReadHandler, Response>::value,
      "OnReadHandler must be invocable with Response");
  static_assert(
      std::is_same<future<bool>, google::cloud::internal::invoke_result_t<
                                     OnReadHandler, Response>>::value,
      "OnReadHandler must return a future<bool>");
  static_assert(
      google::cloud::internal::is_invocable<OnFinishHandler, Status>::value,
      "OnReadHandler must be invocable with Status");

 public:
  explicit AsyncStreamingReadImpl(
      std::unique_ptr<internal::AsyncStreamingReadRpc<Response>> stream,
      OnReadHandler&& on_read, OnFinishHandler&& on_finish)
      : stream_(std::move(stream)),
        on_read_(std::move(on_read)),
        on_finish_(std::move(on_finish)) {}

  void Start() {
    auto self = this->shared_from_this();
    auto start = stream_->Start();
    start.then([self](future<bool> f) {
      // Start was unsuccessful, finish stream.
      if (!f.get()) return self->Finish();
      // Start was successful, start reading.
      self->Read();
    });
  }

 private:
  void Read() {
    auto self = this->shared_from_this();
    auto read = stream_->Read();
    read.then([self](future<absl::optional<Response>> f) {
      auto r = f.get();
      // Read did not yield a response, finish stream.
      if (!r.has_value()) return self->Finish();
      // Read yielded a response, keep reading or drain the stream.
      self->on_read_(*std::move(r)).then([self](future<bool> keep_reading) {
        if (keep_reading.get()) return self->Read();
        self->stream_->Cancel();
        self->Discard();
      });
    });
  }

  void Discard() {
    auto self = this->shared_from_this();
    auto read = stream_->Read();
    read.then([self](future<absl::optional<Response>> f) {
      auto r = f.get();
      // Read did not yield a response, finish stream.
      if (!r.has_value()) return self->Finish();
      // Read yielded a response, keep discarding.
      self->Discard();
    });
  }

  void Finish() {
    auto self = this->shared_from_this();
    auto finish = stream_->Finish();
    finish.then([self](future<Status> f) { self->on_finish_(f.get()); });
  }

  std::unique_ptr<internal::AsyncStreamingReadRpc<Response>> stream_;
  typename std::decay<OnReadHandler>::type on_read_;
  typename std::decay<OnFinishHandler>::type on_finish_;
};

/**
 * Make an asynchronous streaming read RPC.
 *
 * This method performs one loop of an asynchronous streaming read, from
 * `Start()` to `Finish()`. There are callbacks for the caller to process each
 * response, and the final status of the stream.
 *
 * @param stream abstraction for the asynchronous streaming read RPC.
 * @param on_read the callback to be invoked on each successful Read(). If
 * `false` is returned, we will attempt to cancel the stream, and drain the
 *     stream of any subsequent responses.
 * @param on_finish the callback to be invoked when the stream is closed.
 *
 * @tparam Response the type of the response in the async streaming RPC.
 * @tparam OnReadHandler the type of the @p on_read callback.
 * @tparam OnFinishHandler the type of the @p on_finish callback.
 */
template <typename Response, typename OnReadHandler, typename OnFinishHandler>
void PerformAsyncStreamingRead(
    std::unique_ptr<internal::AsyncStreamingReadRpc<Response>> stream,
    OnReadHandler&& on_read, OnFinishHandler&& on_finish) {
  auto loop = std::make_shared<
      AsyncStreamingReadImpl<Response, OnReadHandler, OnFinishHandler>>(
      std::move(stream), std::forward<OnReadHandler>(on_read),
      std::forward<OnFinishHandler>(on_finish));
  loop->Start();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_STREAMING_READ_H
