// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_READ_STREAM_IMPL_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_READ_STREAM_IMPL_H_

#include "google/cloud/bigtable/internal/completion_queue_impl.h"
#include "google/cloud/bigtable/version.h"
#include <memory>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {
/**
 * A meta function to extract the `ResponseType` from an AsyncStreamingReadCall
 * return type.
 *
 * This meta function extracts, if possible, the response type from an
 * asynchronous streaming read RPC callable. These callables return a
 * `std::unique_ptr<grpc::ClientAsyncReaderInterface<T>>` and we are
 * interested in the `T` type.
 *
 * This is the generic version, implementing the "does not match the expected
 * type" path.
 */
template <typename ResponseType>
struct AsyncStreamingReadRpcUnwrap {};

/**
 * A meta function to extract the `ResponseType` from an AsyncStreamingReadCall
 * return type.
 *
 * This meta function extracts, if possible, the response type from an
 * asynchronous streaming read RPC callable. These callables return a
 * `std::unique_ptr<grpc::ClientAsyncReaderInterface<T>>` and we are
 * interested in the `T` type.
 *
 * This is the specialization implementing the "matched with the expected type"
 * path.
 */
template <typename ResponseType>
struct AsyncStreamingReadRpcUnwrap<
    std::unique_ptr<grpc::ClientAsyncReaderInterface<ResponseType>>> {
  using type = ResponseType;
};

/**
 * A meta function to determine the `ResponseType` from an asynchronous\
 * streaming read RPC callable.
 *
 * Asynchronous streaming read RPC calls have the form:
 *
 * @code
 *   std::unique_ptr<grpc::ClientAsyncReaderInterface<ResponseType>>(
 *      grpc::ClientContext*,
 *      RequestType const&,
 *      grpc::CompletionQueue*
 *   );
 * @endcode
 *
 * This meta-function extracts the `ResponseType` given the type of a callable
 * and the `RequestType`.
 */
template <typename AsyncCallType, typename RequestType>
using AsyncStreamingReadResponseType = AsyncStreamingReadRpcUnwrap<
    typename google::cloud::internal::invoke_result_t<
        AsyncCallType, grpc::ClientContext*, RequestType const&,
        grpc::CompletionQueue*>>;

/**
 * Read responses from a asynchronous streaming read RPC and invoke callbacks.
 *
 * This class starts a streaming read RPC, reads all the responses, and invokes
 * the user-provided callbacks for each successful Read() result and the final
 * result for a Finish() request.
 *
 * Objects of this class need to live for as long as there are pending calls
 * on it. We use `std::enable_shared_from_this<>` to keep this object alive
 * until the last callback finishes. This means the objects must always be owned
 * by a `shared_ptr`. This is enforced by making the constructor private and the
 * one factory function returns a `shared_ptr`.
 *
 * @tparam Response the type of the responses in the streaming read RPC.
 * @tparam OnReadHandler the type of the user-provided callable to handle Read
 *     responses.
 * @tparam OnFinishHandler the type of the user-provided callback to handle
 *     Finish responses.
 */
template <typename Response, typename OnReadHandler, typename OnFinishHandler>
class AsyncReadStreamImpl
    : public AsyncOperation,
      public std::enable_shared_from_this<
          AsyncReadStreamImpl<Response, OnReadHandler, OnFinishHandler>> {
 public:
  /**
   * Create a new instance.
   *
   * @param on_read the handler for a successful `Read()` result. Failed
   *   `Read()` operations automatically terminate the loop and call `Finish()`.
   * @param on_finish the handler for a completed `Finish()` result.
   */
  static std::shared_ptr<AsyncReadStreamImpl> Create(
      OnReadHandler&& on_read, OnFinishHandler&& on_finish) {
    return std::shared_ptr<AsyncReadStreamImpl>(
        new AsyncReadStreamImpl(std::forward<OnReadHandler>(on_read),
                                std::forward<OnFinishHandler>(on_finish)));
  }

  /**
   * Start the asynchronous streaming read request and its read loop.
   *
   * @param async_call the function that will make the asynchronous
   *     streaming read RPC.  This is typically a wrapper around one of the
   *     gRPC-generated `PrepareAsync*()` functions.
   * @param request the request parameter for the streaming read RPC.
   * @param context the client context to control the streaming read RPC.
   * @param cq the completion queue that will execute the streaming read RPC. It
   *     is the application's responsibility to keep a thread pool to execute
   *     the completion queue loop.
   *
   * @tparam AsyncFunctionType the type for @p async_call.
   * @tparam Request the type for @p request.
   */
  template <typename AsyncFunctionType, typename Request>
  void Start(AsyncFunctionType&& async_call, Request const& request,
             std::unique_ptr<grpc::ClientContext> context,
             std::shared_ptr<CompletionQueueImpl> cq) {
    // An adapter to call OnStart() via the completion queue.
    class NotifyStart final : public AsyncGrpcOperation {
     public:
      explicit NotifyStart(std::shared_ptr<AsyncReadStreamImpl> c)
          : control_(std::move(c)) {}

     private:
      void Cancel() override {}  // LCOV_EXCL_LINE
      bool Notify(CompletionQueue&, bool ok) override {
        control_->OnStart(ok);
        return true;
      }
      std::shared_ptr<AsyncReadStreamImpl> control_;
    };

    context_ = std::move(context);
    cq_ = std::move(cq);
    reader_ = async_call(context_.get(), request, &cq_->cq());
    auto callback = std::make_shared<NotifyStart>(this->shared_from_this());
    void* tag = cq_->RegisterOperation(std::move(callback));
    reader_->StartCall(tag);
  }

  /// Cancel the current streaming read RPC.
  void Cancel() override { context_->TryCancel(); }

 private:
  /// Handle a completed `Start()` request.
  void OnStart(bool ok) {
    if (!ok) {
      Finish();
      return;
    }
    Read();
  }

  /// Start a `Read()` request.
  void Read() {
    // An adapter to call `OnRead()` via the completion queue.
    class NotifyRead final : public AsyncGrpcOperation {
     public:
      explicit NotifyRead(std::shared_ptr<AsyncReadStreamImpl> c)
          : control_(std::move(c)) {}

      Response response;

     private:
      void Cancel() override {}  // LCOV_EXCL_LINE
      bool Notify(CompletionQueue&, bool ok) override {
        control_->OnRead(ok, std::move(response));
        return true;
      }
      std::shared_ptr<AsyncReadStreamImpl> control_;
    };

    auto callback = std::make_shared<NotifyRead>(this->shared_from_this());
    auto response = &callback->response;
    void* tag = cq_->RegisterOperation(std::move(callback));
    reader_->Read(response, tag);
  }

  /// Handle the result of a `Read()` call.
  void OnRead(bool ok, Response response) {
    if (!ok) {
      Finish();
      return;
    }

    auto continue_reading = on_read_(std::move(response));
    auto self = this->shared_from_this();
    continue_reading.then([self](future<bool> result) {
      if (!result.get()) {
        // Cancel the stream, this is what the user meant by returning `false`.
        self->Cancel();
        // Start discarding messages, gRPC requires that any pending messages
        // are read before calling Finish(). So we need to read until the first
        // message that returns ok==false.
        self->Discard();
        return;
      }
      self->Read();
    });
  }

  /// Start a Finish() request on the underlying read stream.
  void Finish() {
    // An adapter to call `OnFinish()` via the completion queue.
    class NotifyFinish final : public AsyncGrpcOperation {
     public:
      explicit NotifyFinish(std::shared_ptr<AsyncReadStreamImpl> c)
          : control_(std::move(c)) {}

      grpc::Status status;

     private:
      void Cancel() override {}  // LCOV_EXCL_LINE
      bool Notify(CompletionQueue&, bool ok) override {
        control_->OnFinish(ok, MakeStatusFromRpcError(status));
        return true;
      }
      std::shared_ptr<AsyncReadStreamImpl> control_;
    };

    auto callback = std::make_shared<NotifyFinish>(this->shared_from_this());
    auto status = &callback->status;
    void* tag = cq_->RegisterOperation(std::move(callback));
    reader_->Finish(status, tag);
  }

  /// Handle the result of a Finish() request.
  void OnFinish(bool, Status status) { on_finish_(std::move(status)); }

  /**
   * Discard all the messages until OnRead() receives a failure.
   *
   * gRPC requires that `Finish()` be called only once all received values have
   * been discarded. When we cancel a request as a result of `OnRead()`
   * returning false we need to ignore future messages before calling
   * `Finish()`.
   */
  void Discard() {
    // An adapter to call `OnDiscard()` via the completion queue.
    class NotifyDiscard final : public AsyncGrpcOperation {
     public:
      explicit NotifyDiscard(std::shared_ptr<AsyncReadStreamImpl> c)
          : control_(std::move(c)) {}

      Response response;

     private:
      void Cancel() override {}  // LCOV_EXCL_LINE
      bool Notify(CompletionQueue&, bool ok) override {
        control_->OnDiscard(ok, std::move(response));
        return true;
      }
      std::shared_ptr<AsyncReadStreamImpl> control_;
    };

    auto callback = std::make_shared<NotifyDiscard>(this->shared_from_this());
    auto response = &callback->response;
    void* tag = cq_->RegisterOperation(std::move(callback));
    reader_->Read(response, tag);
  }

  /// Handle the result of a Discard() call.
  void OnDiscard(bool ok, Response r) {
    if (!ok) {
      Finish();
      return;
    }
    Discard();
  }

  explicit AsyncReadStreamImpl(OnReadHandler&& on_read,
                               OnFinishHandler&& on_finish)
      : on_read_(std::move(on_read)), on_finish_(std::move(on_finish)) {}

  typename std::decay<OnReadHandler>::type on_read_;
  typename std::decay<OnFinishHandler>::type on_finish_;
  std::unique_ptr<grpc::ClientContext> context_;
  std::shared_ptr<CompletionQueueImpl> cq_;
  std::unique_ptr<grpc::ClientAsyncReaderInterface<Response>> reader_;
};

/**
 * The analogous of `make_shared<>` for `AsyncReadStreamImpl<Response>`.
 *
 * @param on_read the handler for a successful `Read()` result. Failed
 *   `Read()` operations automatically terminate the loop and call `Finish()`.
 * @param on_finish the handler for a completed `Finish()` result.
 *
 * @tparam Response the type of the response.
 * @tparam OnReadHandler the type of @p on_read.
 * @tparam OnFinishHandler the type of @p on_finish.
 */
template <
    typename Response, typename OnReadHandler, typename OnFinishHandler,
    typename std::enable_if<
        google::cloud::internal::is_invocable<OnReadHandler, Response>::value,
        int>::type on_read_is_invocable_with_response = 0,
    typename std::enable_if<
        std::is_same<future<bool>, google::cloud::internal::invoke_result_t<
                                       OnReadHandler, Response>>::value,
        int>::type on_read_returns_future_bool = 0,
    typename std::enable_if<
        google::cloud::internal::is_invocable<OnFinishHandler, Status>::value,
        int>::type on_finish_is_invocable_with_status = 0>
inline std::shared_ptr<
    AsyncReadStreamImpl<Response, OnReadHandler, OnFinishHandler>>
MakeAsyncReadStreamImpl(OnReadHandler&& on_read, OnFinishHandler&& on_finish) {
  return AsyncReadStreamImpl<Response, OnReadHandler, OnFinishHandler>::Create(
      std::forward<OnReadHandler>(on_read),
      std::forward<OnFinishHandler>(on_finish));
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_READ_STREAM_IMPL_H_
