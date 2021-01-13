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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_COMPLETION_QUEUE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_COMPLETION_QUEUE_H

#include "google/cloud/future.h"
#include "google/cloud/internal/async_read_stream_impl.h"
#include "google/cloud/internal/async_rpc_details.h"
#include "google/cloud/internal/completion_queue_impl.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/memory/memory.h"
#include "absl/meta/type_traits.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
class CompletionQueue;

namespace internal {
std::shared_ptr<CompletionQueueImpl> GetCompletionQueueImpl(
    CompletionQueue& cq);
}  // namespace internal

/**
 * Call the functor associated with asynchronous operations when they complete.
 */
class CompletionQueue {
 public:
  CompletionQueue();
  explicit CompletionQueue(std::shared_ptr<internal::CompletionQueueImpl> impl)
      : impl_(std::move(impl)) {}

  /**
   * Run the completion queue event loop.
   *
   * Note that more than one thread can call this member function, to create a
   * pool of threads completing asynchronous operations.
   */
  void Run() { impl_->Run(); }

  /// Terminate the completion queue event loop.
  void Shutdown() { impl_->Shutdown(); }

  /// Cancel all pending operations.
  void CancelAll() { impl_->CancelAll(); }

  /**
   * Create a timer that fires at @p deadline.
   *
   * @param deadline when should the timer expire.
   *
   * @return a future that becomes satisfied after @p deadline.
   *     The result of the future is the time at which it expired, or an error
   *     Status if the timer did not run to expiration (e.g. it was cancelled).
   */
  google::cloud::future<StatusOr<std::chrono::system_clock::time_point>>
  MakeDeadlineTimer(std::chrono::system_clock::time_point deadline) {
    return impl_->MakeDeadlineTimer(deadline);
  }

  /**
   * Create a timer that fires after the @p duration.
   *
   * @tparam Rep a placeholder to match the Rep tparam for @p duration type,
   *     the semantics of this template parameter are documented in
   *     `std::chrono::duration<>` (in brief, the underlying arithmetic type
   *     used to store the number of ticks), for our purposes it is simply a
   *     formal parameter.
   * @tparam Period a placeholder to match the Period tparam for @p duration
   *     type, the semantics of this template parameter are documented in
   *     `std::chrono::duration<>` (in brief, the length of the tick in seconds,
   *     expressed as a `std::ratio<>`), for our purposes it is simply a formal
   *     parameter.
   *
   * @param duration when should the timer expire relative to the current time.
   *
   * @return a future that becomes satisfied after @p duration time has elapsed.
   *     The result of the future is the time at which it expired, or an error
   *     Status if the timer did not run to expiration (e.g. it was cancelled).
   */
  template <typename Rep, typename Period>
  future<StatusOr<std::chrono::system_clock::time_point>> MakeRelativeTimer(
      std::chrono::duration<Rep, Period> duration) {
    return impl_->MakeRelativeTimer(
        std::chrono::duration_cast<std::chrono::nanoseconds>(duration));
  }

  /**
   * Make an asynchronous unary RPC.
   *
   * @param async_call a callable to start the asynchronous RPC.
   * @param request the contents of the request.
   * @param context an initialized request context to make the call.
   *
   * @tparam AsyncCallType the type of @a async_call. It must be invocable with
   *     `(grpc::ClientContext*, RequestType const&, grpc::CompletionQueue*)`.
   *     Furthermore, it should return a
   *     `std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<Response>>>`.
   *     These requirements are verified by
   *     `internal::CheckAsyncUnaryRpcSignature<>`, and this function is
   *     excluded from overload resolution if the parameters do not meet these
   *     requirements.
   * @tparam Request the type of the request parameter in the gRPC.
   *
   * @return a future that becomes satisfied when the operation completes.
   */
  template <
      typename AsyncCallType, typename Request,
      typename Sig = internal::AsyncCallResponseType<AsyncCallType, Request>,
      typename Response = typename Sig::type,
      typename std::enable_if<Sig::value, int>::type = 0>
  future<StatusOr<Response>> MakeUnaryRpc(
      AsyncCallType async_call, Request const& request,
      std::unique_ptr<grpc::ClientContext> context) {
    auto op =
        std::make_shared<internal::AsyncUnaryRpcFuture<Request, Response>>();
    impl_->StartOperation(op, [&](void* tag) {
      op->Start(async_call, std::move(context), request, &impl_->cq(), tag);
    });
    return op->GetFuture();
  }

  /**
   * Make an asynchronous streaming read RPC.
   *
   * Reading from the stream starts automatically, and the handler is notified
   * of all interesting events in the stream. Note that then handler is called
   * by any thread blocked on this object's Run() member function. However, only
   * one callback in the handler is called at a time.
   *
   * @param async_call a callable to start the asynchronous RPC.
   * @param request the contents of the request.
   * @param context an initialized request context to make the call.
   * @param on_read the callback to be invoked on each successful Read().
   * @param on_finish the callback to be invoked when the stream is closed.
   *
   * @tparam AsyncCallType the type of @a async_call. It must be invocable with
   *     parameters
   *     `(grpc::ClientContext*, RequestType const&, grpc::CompletionQueue*)`.
   *     Furthermore, it should return a type convertible to
   *     `std::unique_ptr<grpc::ClientAsyncReaderInterface<Response>>>`.
   *     These requirements are verified by
   *     `internal::AsyncStreamingReadRpcUnwrap<>`, and this function is
   *     excluded from overload resolution if the parameters do not meet these
   *     requirements.
   * @tparam Request the type of the request in the streaming RPC.
   * @tparam Response the type of the response in the streaming RPC.
   * @tparam OnReadHandler the type of the @p on_read callback.
   * @tparam OnFinishHandler the type of the @p on_finish callback.
   */
  template <typename AsyncCallType, typename Request,
            typename Response = typename internal::
                AsyncStreamingReadResponseType<AsyncCallType, Request>::type,
            typename OnReadHandler, typename OnFinishHandler>
  std::shared_ptr<AsyncOperation> MakeStreamingReadRpc(
      AsyncCallType&& async_call, Request const& request,
      std::unique_ptr<grpc::ClientContext> context, OnReadHandler&& on_read,
      OnFinishHandler&& on_finish) {
    auto stream = internal::MakeAsyncReadStreamImpl<Response>(
        std::forward<OnReadHandler>(on_read),
        std::forward<OnFinishHandler>(on_finish));
    stream->Start(std::forward<AsyncCallType>(async_call), request,
                  std::move(context), impl_);
    return stream;
  }

  /**
   * Asynchronously run a functor on a thread `Run()`ning the `CompletionQueue`.
   *
   * @tparam Functor the functor to call on the CompletionQueue thread.
   *   It must satisfy the `void(CompletionQueue&)` signature.
   * @param functor the value of the functor.
   */
  template <typename Functor,
            typename std::enable_if<
                internal::CheckRunAsyncCallback<Functor>::value, int>::type = 0>
  void RunAsync(Functor&& functor) {
    class Wrapper : public internal::RunAsyncBase {
     public:
      Wrapper(std::weak_ptr<internal::CompletionQueueImpl> impl, Functor&& f)
          : impl_(std::move(impl)), fun_(std::forward<Functor>(f)) {}
      ~Wrapper() override = default;
      void exec() override {
        auto impl = impl_.lock();
        if (!impl) return;
        CompletionQueue cq(std::move(impl));
        fun_(cq);
      }

     private:
      std::weak_ptr<internal::CompletionQueueImpl> impl_;
      absl::decay_t<Functor> fun_;
    };
    impl_->RunAsync(
        absl::make_unique<Wrapper>(impl_, std::forward<Functor>(functor)));
  }

  /**
   * Asynchronously run a functor on a thread `Run()`ning the `CompletionQueue`.
   *
   * @tparam Functor the functor to call on the CompletionQueue thread.
   *   It must satisfy the `void()` signature.
   * @param functor the value of the functor.
   */
  template <typename Functor,
            typename std::enable_if<internal::is_invocable<Functor>::value,
                                    int>::type = 0>
  void RunAsync(Functor&& functor) {
    class Wrapper : public internal::RunAsyncBase {
     public:
      explicit Wrapper(Functor&& f) : fun_(std::forward<Functor>(f)) {}
      ~Wrapper() override = default;
      void exec() override { fun_(); }

     private:
      absl::decay_t<Functor> fun_;
    };
    impl_->RunAsync(absl::make_unique<Wrapper>(std::forward<Functor>(functor)));
  }

  /**
   * Asynchronously wait for a connection to become ready.
   *
   * @param channel the channel on which to wait for state changes
   * @param deadline give up waiting for the state change if this deadline
   *     passes
   * @return `future<>` which will be satisfied when either of these events
   *     happen: (a) the connection is ready, (b) the connection permanently
   *     failed, (c) deadline passes before (a) or (b) happen; the future will
   *     be satisfied with `StatusCode::kOk` for (a), `StatusCode::kCancelled`
   *     for (b) and `StatusCode::kDeadlineExceeded` for (c)
   */
  future<Status> AsyncWaitConnectionReady(
      std::shared_ptr<grpc::Channel> channel,
      std::chrono::system_clock::time_point deadline);

 private:
  friend std::shared_ptr<internal::CompletionQueueImpl>
  internal::GetCompletionQueueImpl(CompletionQueue& cq);
  std::shared_ptr<internal::CompletionQueueImpl> impl_;
};

namespace internal {

inline std::shared_ptr<CompletionQueueImpl> GetCompletionQueueImpl(
    CompletionQueue& cq) {
  return cq.impl_;
}

}  // namespace internal

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_COMPLETION_QUEUE_H
