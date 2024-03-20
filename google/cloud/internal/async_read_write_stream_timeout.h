// Copyright 2024 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_READ_WRITE_STREAM_TIMEOUT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_READ_WRITE_STREAM_TIMEOUT_H

#include "google/cloud/async_streaming_read_write_rpc.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/version.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * A decorator to timeout each `Write*()` and `Read()` call in a streaming
 * read-write RPC.
 *
 * Streaming read-write RPCs are often used to send and/or receive large amounts
 * of data. The total amount of data transferred in a single RPC is often
 * unknown at the time request is started. The canonical example of these
 * requests is "upload a GCS object" or "read a GCS object".
 *
 * An absolute timeout for these requests is very hard to get right. Set the
 * timeout too small, and large requests timeout when they shouldn't. Set the
 * timeout too large, and the request may stall and this goes undetected for
 * too long.
 *
 * gRPC only allows setting timeouts when the request is started, and the size
 * of the request is unknown at that time. We need a different mechanism to
 * detect stalled streaming RPCS.
 *
 * We prefer to estimate a "per write" and "per read" timeout. The size of
 * each message, and thus their expected transfer times are better known. These
 * are still estimations, but we can set a conservative limit; something that
 * implies a minimum "bytes per second" rate. For example, setting the limit to
 * 10s in Google Cloud Storage implies a minimum rate of 200 KiB/s, which is
 * about 3 orders of magnitude smaller than the observed upload rate.
 */
template <typename Request, typename Response>
class AsyncStreamingReadWriteRpcTimeout
    : public AsyncStreamingReadWriteRpc<Request, Response> {
 public:
  AsyncStreamingReadWriteRpcTimeout(
      CompletionQueue cq, std::chrono::milliseconds start_timeout,
      std::chrono::milliseconds per_read_timeout,
      std::chrono::milliseconds per_write_timeout,
      std::unique_ptr<AsyncStreamingReadWriteRpc<Request, Response>> child)
      : state_(std::make_shared<State>(std::move(cq), start_timeout,
                                       per_read_timeout, per_write_timeout,
                                       std::move(child))) {}

  void Cancel() override { state_->child->Cancel(); }

  future<bool> Start() override { return state_->Start(); }

  future<absl::optional<Response>> Read() override { return state_->Read(); }

  future<bool> Write(Request const& request,
                     grpc::WriteOptions options) override {
    return state_->Write(request, std::move(options));
  }

  future<bool> WritesDone() override { return state_->WritesDone(); }

  future<Status> Finish() override { return state_->child->Finish(); }

  RpcMetadata GetRequestMetadata() const override {
    return state_->child->GetRequestMetadata();
  }

 private:
  // We need to keep the state in a separate object because we will be using
  // `shared_from_this()` (and `WeakFromThis()`) to setup callbacks. Requiring
  // `AsyncStreamingReadWriteRpcTimeout` to be a `shared_ptr<>` would require
  // changes to all the other decorators.
  struct State : public std::enable_shared_from_this<State> {
    State(CompletionQueue c, std::chrono::milliseconds s,
          std::chrono::milliseconds r, std::chrono::milliseconds w,
          std::unique_ptr<AsyncStreamingReadWriteRpc<Request, Response>> ch)
        : cq(std::move(c)),
          start_timeout(s),
          per_read_timeout(r),
          per_write_timeout(w),
          child(std::move(ch)) {}

    std::weak_ptr<State> WeakFromThis() { return this->shared_from_this(); }

    future<bool> Start() {
      auto watchdog = CreateWatchdog(start_timeout);
      return child->Start().then(
          [watchdog = std::move(watchdog), w = WeakFromThis()](auto f) mutable {
            if (auto self = w.lock()) {
              return self->OnStart(std::move(watchdog), f.get());
            }
            return make_ready_future(false);
          });
    }

    future<bool> OnStart(future<bool> watchdog, bool start) {
      watchdog.cancel();
      return watchdog.then([w = WeakFromThis(), start](auto f) {
        auto expired = f.get();
        return !expired && start;
      });
    }

    future<absl::optional<Response>> Read() {
      auto watchdog = CreateWatchdog(per_read_timeout);
      return child->Read().then(
          [wd = std::move(watchdog), w = WeakFromThis()](auto f) mutable {
            if (auto self = w.lock())
              return self->OnRead(std::move(wd), f.get());
            return make_ready_future(absl::optional<Response>{});
          });
    }

    future<absl::optional<Response>> OnRead(future<bool> watchdog,
                                            absl::optional<Response> response) {
      watchdog.cancel();
      return watchdog.then(
          [w = WeakFromThis(), r = std::move(response)](auto f) mutable {
            auto expired = f.get();
            if (expired) return absl::optional<Response>{};
            return std::move(r);
          });
    }

    future<bool> Write(Request const& request, grpc::WriteOptions wo) {
      auto watchdog = CreateWatchdog(per_write_timeout);
      return child->Write(request, std::move(wo))
          .then([wd = std::move(watchdog), w = WeakFromThis()](auto f) mutable {
            if (auto self = w.lock()) {
              return self->OnWrite(std::move(wd), f.get());
            }
            return make_ready_future(false);
          });
    }

    future<bool> OnWrite(future<bool> watchdog, bool write) {
      watchdog.cancel();
      return watchdog.then([w = WeakFromThis(), write](auto f) mutable {
        auto expired = f.get();
        return !expired && write;
      });
    }

    future<bool> WritesDone() {
      auto watchdog = CreateWatchdog(per_write_timeout);
      return child->WritesDone().then(
          [wd = std::move(watchdog), w = WeakFromThis()](auto f) mutable {
            if (auto self = w.lock()) {
              return self->OnWritesDone(std::move(wd), f.get());
            }
            return make_ready_future(false);
          });
    }

    future<bool> OnWritesDone(future<bool> watchdog, bool done) {
      watchdog.cancel();
      return watchdog.then([w = WeakFromThis(), done](auto f) mutable {
        auto expired = f.get();
        return !expired && done;
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
    std::chrono::milliseconds per_write_timeout;
    std::unique_ptr<AsyncStreamingReadWriteRpc<Request, Response>> child;
  };
  std::shared_ptr<State> state_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_READ_WRITE_STREAM_TIMEOUT_H
