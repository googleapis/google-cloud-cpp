// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/async_accumulate_read_object.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

/**
 * Keeps the state for `AsyncAccumulateReadObjectPartial()`.
 *
 * This is analogous to a C++20 coroutine handle. It keeps the state for
 * `AsyncAccumulateReadObjectPartial()`, that is, all the function call
 * arguments, as well as any local variables.
 *
 * Whereas in C++20 coroutines we would use `co_await` to suspend execution,
 * here we need to use a callback, so code like:
 *
 * @code
 *   auto x = co_await Foo();
 * @endcode
 *
 * becomes
 *
 * @code
 *   Foo().then([self = shared_from_this()](auto f) { self->OnFoo(f.get()); });
 * @endcode
 *
 * An extra complication is that we use timeouts, so while a naive
 * implementation of this code would say:
 *
 * @code
 *   auto read = co_await stream->Read();
 * @endcode
 *
 * here we launch two coroutines, one to run the timer, and one to actually
 * make the call subject to a timeout. The trick is to set the timeout to cancel
 * the full streaming RPC if it expires successfully, and to cancel the timer
 * if the operation completes:
 *
 * @code
 *   auto tm = cq.MakeRelativeTimer(timeout).then([&stream](auto f) {
 *     if (!f.get().ok()) return false; // timer canceled, nothing to do
 *     stream->Cancel();
 *     return true;
 *   });
 *   auto read = co_await stream->Read();
 *   tm.cancel();
 *   if (co_await tm) { // handle the timeout
 *   // success!
 * @endcode
 *
 * Without coroutines this requires passing the timer future to the `OnRead()`
 * callback. See below for the details.
 */
class AsyncAccumulateReadObjectPartialHandle
    : public std::enable_shared_from_this<
          AsyncAccumulateReadObjectPartialHandle> {
 public:
  using Response = ::google::storage::v2::ReadObjectResponse;
  using Stream = ::google::cloud::internal::AsyncStreamingReadRpc<Response>;
  using StreamingRpcMetadata = ::google::cloud::internal::StreamingRpcMetadata;
  using Result = AsyncAccumulateReadObjectResult;

  AsyncAccumulateReadObjectPartialHandle(CompletionQueue cq,
                                         std::unique_ptr<Stream> stream,
                                         std::chrono::milliseconds timeout)
      : cq_(std::move(cq)), stream_(std::move(stream)), timeout_(timeout) {}

  future<Result> Invoke() {
    struct ByMove {
      std::shared_ptr<AsyncAccumulateReadObjectPartialHandle> self;
      future<bool> tm;
      void operator()(future<bool> f) { self->OnStart(std::move(tm), f.get()); }
    };
    auto tm = MakeTimeout();
    stream_->Start().then(ByMove{shared_from_this(), std::move(tm)});
    return promise_.get_future();
  }

 private:
  void OnStart(future<bool> tm, bool ok) {
    tm.cancel();
    if (tm.get()) return OnTimeout("Start()");
    if (!ok) return Finish();
    Read();
  }

  void Read() {
    struct ByMove {
      std::shared_ptr<AsyncAccumulateReadObjectPartialHandle> self;
      future<bool> tm;
      void operator()(future<absl::optional<Response>> f) {
        self->OnRead(std::move(tm), f.get());
      }
    };
    auto tm = MakeTimeout();
    stream_->Read().then(ByMove{shared_from_this(), std::move(tm)});
  }

  void OnRead(future<bool> tm, absl::optional<Response> response) {
    tm.cancel();
    if (tm.get()) return OnTimeout("Read()");
    if (!response.has_value()) return Finish();
    accumulator_.push_back(*std::move(response));
    Read();
  }

  void Finish() {
    struct ByMove {
      std::shared_ptr<AsyncAccumulateReadObjectPartialHandle> self;
      future<bool> tm;
      void operator()(future<Status> f) {
        self->OnFinish(std::move(tm), f.get());
      }
    };
    auto tm = MakeTimeout();
    stream_->Finish().then(ByMove{shared_from_this(), std::move(tm)});
  }

  void OnFinish(future<bool> tm, Status status) {
    tm.cancel();
    promise_.set_value(Result{std::move(accumulator_),
                              stream_->GetRequestMetadata(),
                              std::move(status)});
  }

  future<bool> MakeTimeout() {
    auto self = shared_from_this();
    return cq_.MakeRelativeTimer(timeout_).then(
        [self](future<StatusOr<std::chrono::system_clock::time_point>> f) {
          if (!f.get().ok()) return false;
          self->stream_->Cancel();
          return true;
        });
  }
  void OnTimeout(char const* where) {
    auto finish = stream_->Finish();
    finish.then(WaitForFinish{std::move(stream_)});
    promise_.set_value(
        Result{std::move(accumulator_),
               google::cloud::internal::StreamingRpcMetadata{},
               Status(StatusCode::kDeadlineExceeded,
                      std::string{"Timeout waiting for "} + where)});
  }

  // Assume ownership of `stream` until its `Finish()` callback completes.
  struct WaitForFinish {
    std::unique_ptr<Stream> stream;
    void operator()(future<Status>) const {}
  };

  promise<Result> promise_;
  std::vector<Response> accumulator_;
  CompletionQueue cq_;
  std::unique_ptr<Stream> stream_;
  std::chrono::milliseconds timeout_;
};

}  // namespace

future<AsyncAccumulateReadObjectResult> AsyncAccumulateReadObjectPartial(
    CompletionQueue cq,
    std::unique_ptr<google::cloud::internal::AsyncStreamingReadRpc<
        google::storage::v2::ReadObjectResponse>>
        stream,
    std::chrono::milliseconds timeout) {
  auto handle = std::make_shared<AsyncAccumulateReadObjectPartialHandle>(
      std::move(cq), std::move(stream), timeout);
  return handle->Invoke();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
