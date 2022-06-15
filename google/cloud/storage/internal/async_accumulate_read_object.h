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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_ACCUMULATE_READ_OBJECT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_ACCUMULATE_READ_OBJECT_H

#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/async_streaming_read_rpc.h"
#include "google/cloud/version.h"
#include <google/storage/v2/storage.pb.h>
#include <chrono>
#include <memory>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Implement an asynchronous loop to accumulate the data returned by
 * `AsyncReadObject()`.
 *
 * The implementation of `AsyncClient::ReadObject()` needs to accumulate the
 * results of one or more `ReadObject()` requests (which are streaming read
 * RPCs) and return a single `future<T>` to the application. The implementation
 * must also automatically resume interrupted calls, and restart the download
 * from the last received byte.
 *
 * If we were using C++20, this would be a coroutine, and we will use that
 * coroutine to explain what this code does.  First the easy version:
 *
 * @code
 * using ::google::storage::v2::ReadResponse;
 * using Stream =
 *     ::google::cloud::internal::AsyncStreamingReadRpc<ReadResponse>;
 * future<std::pair<Status, std::vector<ReadResponse>>>
 * AsyncAccumulateReadObject(std::unique_ptr<Stream> stream) {
 *   std::vector<ReadResponse> accumulator;
 *   auto start = co_await stream->Start();
 *   while (start) {
 *     absl::optional<ReadResponse> value = co_await stream->Read();
 *     if (!value) break;
 *     accumulator.push_back(*std::move(value));
 *   }
 *   auto finish = co_await stream->Finish();
 *   co_return std::make_pair(std::move(finish), std::move(response));
 * }
 * @endcode
 *
 * This is pretty straight forward.  To support C++ <= 17, we need to convert
 * this into an object.  Each place where there is a `co_await` goes through
 * a transformation like so:
 *
 * // This replaces the stream->Start() line
 * void AsyncAccumulateReadObject::Start() {
 *   auto self = shared_from_this();
 *   stream_->Start().then([self](auto f) { self->OnStart(f.get()); });
 * }
 *
 * void AsyncAccumulateReadObject::OnStart(bool ok) {
 *   if (!ok) return Finish();
 *   Read();
 * }
 *
 * But we need to take into account timeouts. For downloads, it is not the total
 * timeout that matters (downloading 1 TiB of data should take a long time, but
 * whether the download is making progress. That is, we need to timeout each
 * `Start()`, `Read()` and `Finish()` operation.  We will illustrate how this
 * works by just adding the timeouts around `Read(), using coroutines first:
 *
 * @code
 * using ::google::storage::v2::ReadResponse;
 * using Stream =
 *     ::google::cloud::internal::AsyncStreamingReadRpc<ReadResponse>;
 *
 * future<bool> CancelOnTimeout(
 *     Stream& stream, CompletionQueue& cq, std::chrono::milliseconds timeout) {
 *   return cq.MakeRelativeTimeout(timeout).then([&stream](auto f) {
 *     if (!f.get().ok()) return false;  // false is the timer is cancelled
 *     stream.Cancel();
 *     return true;
 *   });
 * }
 *
 * // Close the stream after a timeout.
 * future<void> HandleTimeout(std::unique_ptr<Stream>);
 *
 * future<std::pair<Status, std::vector<ReadResponse>>>
 * AsyncAccumulateReadObject(
 *     CompletionQueue cq, std::unique_ptr<Stream> stream,
 *     std::chrono::milliseconds timeout) {
 *   std::vector<ReadResponse> accumulator;
 *   auto start = co_await stream->Start();
 *   while (start) {
 *     auto tm = CancelOnTimeout(*stream, cq, timeout);
 *     absl::optional<ReadResponse> value = co_await stream->Read();
 *     tm.cancel();
 *     if (co_await tm) {
 *       // The timer expired before the operation completed, return a timeout.
 *       HandleTimeout(std::move(stream));
 *       return std::make_pair(
 *           Status(StatusCode::kDeadlineExceeded, "..."), std::move(response));
 *     }
 *     if (!value) break;
 *     accumulator.push_back(*std::move(value));
 *   }
 *   auto finish = co_await stream->Finish();
 *   co_return std::make_pair(std::move(finish), std::move(response));
 * }
 * @endcode
 *
 * This becomes a more serious set of callbacks:
 *
 * @code
 * // This replaces the stream->Start() line
 * void AsyncAccumulateReadObject::Read() {
 *   auto self = shared_from_this();
 *   auto tm = cq_->MakeRelativeTimer(timeout_).then([self](auto f) {
 *       if (!f.ok()) return false;
 *       stream_->Cancel();
 *       return true;
 *   });
 *   stream_->Start().then([self, tm = std::move(tm)](auto f) mutable {
 *     self->OnStart(std::move(tm), f.get());
 *   });
 * }
 *
 * void AsyncAccumulateReadObject::OnRead(
 *     future<bool> tm, absl::optional<ReadResponse> response) {
 *   tm.cancel();
 *   if (tm.get()) {
 *     HandleTimeout(std::move(stream_));
 *     promise_.set_value(std::make_pair(
 *       Status(StatusCode::kDeadlineExceeded, "..."), std::move(response_));
 *     return;
 *   }
 *   if (!response.has_value()) return Finish();
 *   accumulator.push_back(*std::move(response));
 *   Read();
 * }
 * @endcode
 *
 */
class AsyncAccumulateReadObject
    : public std::enable_shared_from_this<AsyncAccumulateReadObject> {
 public:
  using Response = ::google::storage::v2::ReadObjectResponse;
  using Stream = ::google::cloud::internal::AsyncStreamingReadRpc<Response>;
  using StreamingRpcMetadata = ::google::cloud::internal::StreamingRpcMetadata;
  struct Result {
    Status status;
    std::vector<Response> payload;
    StreamingRpcMetadata metadata;
  };

  static future<Result> Start(CompletionQueue cq,
                              std::unique_ptr<Stream> stream,
                              std::chrono::milliseconds timeout);

 private:
  AsyncAccumulateReadObject(CompletionQueue cq, std::unique_ptr<Stream> stream,
                            std::chrono::milliseconds timeout);

  void DoStart();
  void OnStart(future<bool> tm, bool ok);
  void Read();
  void OnRead(future<bool> tm, absl::optional<Response> response);
  void Finish();
  void OnFinish(future<bool> tm, Status status);

  future<bool> MakeTimeout();
  void OnTimeout(char const* where);

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

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_ACCUMULATE_READ_OBJECT_H
