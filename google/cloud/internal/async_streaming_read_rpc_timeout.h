// Copyright 2023 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_STREAMING_READ_RPC_TIMEOUT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_STREAMING_READ_RPC_TIMEOUT_H

#include "google/cloud/completion_queue.h"
#include "google/cloud/internal/async_streaming_read_rpc.h"
#include "google/cloud/version.h"
#include <chrono>
#include <memory>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * A decorator to timeout each `Read()` call in a streaming read RPC.
 *
 * Streaming read RPCs are often used to return large amounts of data, often
 * unknown at the time request is made. Examples of these requests include
 * "download this GCS object" or "read all the rows in this table".
 *
 * An absolute timeout for these requests is very hard to get right. Set the
 * timeout too small, and large responses timeout when they shouldn't. Set the
 * timeout too large, and the response may stall and this goes undetected for
 * too long.
 *
 * Because the size of the response is unknown when the request is made, and
 * gRPC only allows setting timeouts when the request is configured we need
 * a different mechanism to detect stalled streaming RPCS.
 *
 * We prefer to estimate a "per read timeout". This is still an estimation, but
 * we can set a conservative limit; something that implies a minimum
 * "bytes per second" or "rows per second" rate. For example, setting the
 * limit to 10s in Google Cloud Storage implies a minimum rate of 200 KiB/s,
 * which is 3 orders of magnitude smaller than the observed download rate.
 */
template <typename Response>
class AsyncStreamingReadRpcTimeout : public AsyncStreamingReadRpc<Response> {
 public:
  AsyncStreamingReadRpcTimeout(
      CompletionQueue cq, std::chrono::milliseconds start_timeout,
      std::chrono::milliseconds per_read_timeout,
      std::unique_ptr<AsyncStreamingReadRpc<Response>> child)
      : state_(std::make_shared<State>(std::move(cq), start_timeout,
                                       per_read_timeout, std::move(child))) {}

  void Cancel() override { state_->child->Cancel(); }

  future<bool> Start() override { return state_->Start(); }

  future<absl::optional<Response>> Read() override { return state_->Read(); }

  future<Status> Finish() override { return state_->child->Finish(); }

  RpcMetadata GetRequestMetadata() const override {
    return state_->child->GetRequestMetadata();
  }

 private:
  // We need to keep the state in a separate object because we will be using
  // `shared_from_this()` (and `WeakFromThis()`) to setup callbacks. Requiring
  // `AsyncStreamingReadRpcTimeout` to be a `shared_ptr<>` would require
  // changes to all the other decorators.
  struct State : public std::enable_shared_from_this<State> {
    State(CompletionQueue c, std::chrono::milliseconds s,
          std::chrono::milliseconds p,
          std::unique_ptr<AsyncStreamingReadRpc<Response>> ch)
        : cq(std::move(c)),
          start_timeout(s),
          per_read_timeout(p),
          child(std::move(ch)) {}

    std::weak_ptr<State> WeakFromThis() { return this->shared_from_this(); }

    future<bool> Start() {
      auto watchdog = CreateWatchdog(start_timeout);
      return child->Start().then(
          [watchdog = std::move(watchdog), w = WeakFromThis()](auto f) mutable {
            if (auto self = w.lock())
              return self->OnStart(std::move(watchdog), f.get());
            return make_ready_future(false);
          });
    }

    future<bool> OnStart(future<bool> watchdog, bool ok) {
      watchdog.cancel();
      return watchdog.then([w = WeakFromThis(), ok](auto f) {
        auto expired = f.get();
        return ok && !expired;
      });
    }

    future<absl::optional<Response>> Read() {
      auto watchdog = CreateWatchdog(per_read_timeout);
      return child->Read().then(
          [watchdog = std::move(watchdog), w = WeakFromThis()](auto f) mutable {
            if (auto self = w.lock())
              return self->OnRead(std::move(watchdog), f.get());
            return make_ready_future(absl::optional<Response>());
          });
    }

    future<absl::optional<Response>> OnRead(future<bool> watchdog,
                                            absl::optional<Response> read) {
      watchdog.cancel();
      return watchdog.then(
          [w = WeakFromThis(), read = std::move(read)](auto f) mutable {
            auto expired = f.get();
            if (expired) return absl::optional<Response>();
            return std::move(read);
          });
    }

    future<bool> CreateWatchdog(std::chrono::milliseconds timeout) {
      if (timeout == std::chrono::milliseconds(0)) {
        return make_ready_future(false);
      }

      return cq.MakeRelativeTimer(timeout).then([w = WeakFromThis()](auto f) {
        if (auto self = w.lock()) return self->OnTimer(f.get().ok());
        return false;
      });
    }

    bool OnTimer(bool expired) {
      if (expired) child->Cancel();
      return expired;
    }

    CompletionQueue cq;
    std::chrono::milliseconds start_timeout;
    std::chrono::milliseconds per_read_timeout;
    std::unique_ptr<AsyncStreamingReadRpc<Response>> child;
  };
  std::shared_ptr<State> state_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_STREAMING_READ_RPC_TIMEOUT_H
