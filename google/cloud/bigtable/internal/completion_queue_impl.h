// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_COMPLETION_QUEUE_IMPL_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_COMPLETION_QUEUE_IMPL_H_

#include "google/cloud/bigtable/async_operation.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/internal/throw_delegate.h"
#include <grpcpp/alarm.h>
#include <grpcpp/support/async_stream.h>
#include <grpcpp/support/async_unary_call.h>
#include <atomic>
#include <string>
#include <unordered_map>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
class CompletionQueue;
namespace internal {

/**
 * Represents an AsyncOperation which gRPC understands.
 *
 * When applications create an asynchronous operations with a `CompletionQueue`
 * they provide a callback to be invoked when the operation completes
 * (successfully or not). The completion queue type-erases the callback and
 * hides it in a class derived from `AsyncOperation`. A shared pointer to the
 * `AsyncOperation` is returned by the completion queue so library developers
 * can cancel the operation if needed.
 */
class AsyncGrpcOperation : public AsyncOperation {
 private:
  friend class internal::CompletionQueueImpl;
  /**
   * Notifies the application that the operation completed.
   *
   * Derived classes wrap the callbacks provided by the application and invoke
   * the callback when this virtual member function is called.
   *
   * @param cq the completion queue sending the notification, this is useful in
   *   case the callback needs to retry the operation.
   * @param ok opaque parameter returned by grpc::CompletionQueue.  The
   *   semantics defined by gRPC depend on the type of operation, therefore the
   *   operation needs to interpret this parameter based on those semantics.
   * @return Whether the operation is completed (e.g. in case of streaming
   *   response, it would return true only after the stream is finished).
   */
  virtual bool Notify(CompletionQueue& cq, bool ok) = 0;
};

/**
 * Tests if @p Functor meets the requirements for a timer callback.
 *
 * @tparam Functor a type the application wants to use as a timer callback.
 */
template <typename Functor>
using CheckTimerCallback =
    google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                          AsyncTimerResult&>;

/**
 * Wrap a timer callback into an `AsyncOperation`.
 *
 * Applications (or more likely, other components in the client library) will
 * associate callbacks of many different types with a completion queue. This
 * class is created by the completion queue implementation to type-erase the
 * callbacks, and thus be able to treat them homogenously in the completion
 * queue. Note that this class lives in the `internal` namespace and thus is
 * not intended for general use.
 *
 * @tparam Functor the callback type.
 */
template <
    typename Functor,
    typename std::enable_if<CheckTimerCallback<Functor>::value, int>::type = 0>
class AsyncTimerFunctor : public AsyncGrpcOperation {
 public:
  explicit AsyncTimerFunctor(Functor functor,
                             std::unique_ptr<grpc::Alarm> alarm)
      : functor_(std::move(functor)), alarm_(std::move(alarm)) {}

  void Set(grpc::CompletionQueue& cq,
           std::chrono::system_clock::time_point deadline, void* tag) {
    std::unique_lock<std::mutex> lk(mu_);
    timer_.deadline = deadline;
    if (alarm_) {
      alarm_->Set(&cq, deadline, tag);
    }
  }

  void Cancel() override {
    std::unique_lock<std::mutex> lk(mu_);
    if (alarm_) {
      alarm_->Cancel();
    }
  }

 private:
  bool Notify(CompletionQueue& cq, bool ok) override {
    std::unique_lock<std::mutex> lk(mu_);
    alarm_.reset();
    timer_.cancelled = !ok;
    lk.unlock();
    // At this point timer_ is not going to be used by any other thread, so we
    // might use it without a lock.
    functor_(cq, timer_);
    return true;
  }

  // It might not be clear why the mutex is needed.
  // We need to make sure that `if (alarm_) { alarm_->Cancel(); }` is atomic.
  // Without the mutex it is not because `Notify` resets `alarm_`.
  std::mutex mu_;
  Functor functor_;
  AsyncTimerResult timer_;
  std::unique_ptr<grpc::Alarm> alarm_;
};

/// Verify that @p Functor meets the requirements for an AsyncUnaryRpc callback.
template <typename Functor, typename Response>
using CheckUnaryRpcCallback =
    google::cloud::internal::is_invocable<Functor, CompletionQueue&, Response&,
                                          grpc::Status&>;

/**
 * Verify that @p Functor meets the requirements for an AsyncUnaryStreamRpc
 * data callback.
 */
template <typename Functor, typename Response>
using CheckUnaryStreamRpcDataCallback = google::cloud::internal::is_invocable<
    Functor, CompletionQueue&, const grpc::ClientContext&, Response&>;

/**
 * Verify that @p Functor meets the requirements for an AsyncUnaryStreamRpc
 * finishing callback.
 */
template <typename Functor, typename Response>
using CheckUnaryStreamRpcFinishedCallback =
    google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                          grpc::ClientContext&, grpc::Status&>;
/**
 * Wrap a unary RPC callback into a `AsyncOperation`.
 *
 * Applications (or more likely other components in the client library) will
 * associate callbacks of many different types with a completion queue. This
 * class is created by the completion queue implementation to type-erase the
 * callbacks, and thus be able to treat them homogenously in the completion
 * queue. Note that this class lives in the `internal` namespace and thus is
 * not intended for general use.
 *
 * @tparam Request the type of the RPC request.
 * @tparam Response the type of the RPC response.
 * @tparam Functor the callback type.
 */
template <typename Request, typename Response, typename Functor,
          typename std::enable_if<
              CheckUnaryRpcCallback<Functor, Response>::value, int>::type = 0>
class AsyncUnaryRpcFunctor : public AsyncGrpcOperation {
 public:
  explicit AsyncUnaryRpcFunctor(Functor functor)
      : sync_(false), functor_(std::move(functor)) {}

  /// Make the RPC request and prepare the response callback.
  template <typename Client, typename MemberFunction>
  void Set(Client& client, MemberFunction Client::*call,
           std::unique_ptr<grpc::ClientContext> context, Request const& request,
           grpc::CompletionQueue* cq, void* tag) {
    context_ = std::move(context);
    // Make sure context_ is visible in other threads.
    sync_.store(0, std::memory_order_release);
    auto rpc = (client.*call)(context_.get(), request, cq);
    rpc->Finish(&response_, &status_, tag);
  }

  void Cancel() override {
    // Make sure context_ is visible in this thread.
    static_cast<void>(sync_.load(std::memory_order_acquire));
    context_->TryCancel();
  }

 private:
  bool Notify(CompletionQueue& cq, bool ok) override {
    // Make sure members are visible.
    static_cast<void>(sync_.load(std::memory_order_acquire));
    if (!ok) {
      // This would mean a bug in grpc. Documentation states that Finish()
      // always returns true.
      status_ =
          grpc::Status(grpc::StatusCode::UNKNOWN, "Finish() returned false");
    }
    functor_(cq, response_, status_);
    return true;
  }

  // sync_ doesn't have any semantics. It is used to generate proper barriers
  // for synchronization.
  std::atomic<bool> sync_;
  std::unique_ptr<grpc::ClientContext> context_;
  Functor functor_;
  grpc::Status status_;
  Response response_;
};

/**
 * Unary RPC with streaming response AsyncOperation wrapper.
 *
 * This is AsyncUnaryRpcFunctor's counterpart for RPCs with streaming responses.
 * It encapsulates the stream's state machine and allows specifying
 * callbacks for data portions and end-of-stream.
 *
 * Note that this class lives in the `internal` namespace and thus is
 * not intended for general use.
 *
 * @tparam Request the type of the RPC request.
 * @tparam Response the type of the RPC response piece.
 * @tparam DataFunctor the callback type for notifying about data protions.
 * @tparam FinishedFunctor the callback type for notifying about end of stream.
 */
template <typename Request, typename Response, typename DataFunctor,
          typename FinishedFunctor,
          typename std::enable_if<
              CheckUnaryStreamRpcDataCallback<DataFunctor, Response>::value,
              int>::type = 0,
          typename std::enable_if<CheckUnaryStreamRpcFinishedCallback<
                                      FinishedFunctor, Response>::value,
                                  int>::type = 0>
class AsyncUnaryStreamRpcFunctor : public AsyncGrpcOperation {
 public:
  explicit AsyncUnaryStreamRpcFunctor(DataFunctor data_functor,
                                      FinishedFunctor finished_functor)
      : tag_(nullptr),
        state_(CREATING),
        data_functor_(std::move(data_functor)),
        finished_functor_(std::move(finished_functor)) {}

  /// Make the RPC request and prepare the response callback.
  template <typename Client, typename MemberFunction>
  void Set(Client& client, MemberFunction Client::*call,
           std::unique_ptr<grpc::ClientContext> context, Request const& request,
           grpc::CompletionQueue* cq, void* tag) {
    std::unique_lock<std::mutex> lk(mu_);
    tag_ = tag;
    context_ = std::move(context);
    response_reader_ = (client.*call)(context_.get(), request, cq, tag);
  }

  void Cancel() override {
    std::unique_lock<std::mutex> lk(mu_);
    context_->TryCancel();
  }

 private:
  enum State { CREATING, PROCESSING, FINISHING };
  bool Notify(CompletionQueue& cq, bool ok) override {
    std::unique_lock<std::mutex> lk(mu_);

    switch (state_) {
      case CREATING:
        if (ok) {
          response_reader_->Read(&response_, tag_);
          state_ = PROCESSING;
        } else {
          response_reader_->Finish(&status_, tag_);
          state_ = FINISHING;
        }
        return false;
      case PROCESSING:
        if (ok) {
          Response received;
          response_.Swap(&received);
          lk.unlock();
          // The simple way to assure that we don't reorder callbacks is not
          // submitting the next Read() until the user callback finishes.
          data_functor_(cq, *context_, received);
          lk.lock();
          // We must hold the lock while calling Read(): this operation may
          // trigger a callback in other threads running the completion event
          // queue, and those should be blocked until this function returns.  On
          // the other hand, Read() should not block for a long time: gRPC says
          // that this is an asynchronous operation, blocking for a long time
          // would make it impossible to write asynchronous applications
          // efficiently.
          response_reader_->Read(&response_, tag_);
        } else {
          response_reader_->Finish(&status_, tag_);
          state_ = FINISHING;
        }
        return false;
      case FINISHING:
        lk.unlock();
        finished_functor_(cq, *context_, status_);
        return true;
    }
    google::cloud::internal::ThrowRuntimeError(
        "unexpected state in AsyncUnaryStreamRpcFunctor: " +
        std::to_string(state_));
  }

  // It is not obvious why the mutex is used. There are 4 reasons:
  //
  // Cancel() might be called after Start() in a different thread. We need a
  // synchronization point to make sure that it has the context to act on.
  //
  // Changes to members in `Set()` need to be reflected in what `Notify()` sees.
  //
  // Read()s should not be run concurrently on response_reader_. There is no
  // guarantee that a thread is going to get Notify()ed only after Read() is
  // fully finished.
  //
  // The mutex also acts as a barrier here to make sure that whatever is written
  // to this object's fields is visible in threads which get subsequently
  // Notify()ed.
  //
  // The likelihood of some of these reasons materializing is low, but it's
  // better to be on the safe side.
  std::mutex mu_;
  void* tag_;
  State state_;
  grpc::Status status_;
  DataFunctor data_functor_;
  FinishedFunctor finished_functor_;
  Response response_;
  std::unique_ptr<grpc::ClientContext> context_;
  std::unique_ptr<grpc::ClientAsyncReaderInterface<Response>> response_reader_;
};

template <typename T>
struct ExtractMemberFunctionType : public std::false_type {
  using ClassType = void;
  using MemberFunctionType = void;
};

template <typename ClassType, typename MemberFunctionType>
struct ExtractMemberFunctionType<MemberFunctionType ClassType::*>
    : public std::true_type {
  using Class = ClassType;
  using MemberFunction = MemberFunctionType;
};

/// Determine the Request and Response parameter for an RPC based on the Stub
/// signature - mismatch case.
template <typename MemberFunction>
struct CheckAsyncUnaryRpcSignature : public std::false_type {
  using RequestType = int;
  using ResponseType = int;
};

/// Determine the Request and Response parameter for an RPC based on the Stub
/// signature - match case.
template <typename Request, typename Response>
struct CheckAsyncUnaryRpcSignature<
    std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<Response>>(
        grpc::ClientContext*, Request const&, grpc::CompletionQueue*)>
    : public std::true_type {
  using RequestType = Request;
  using ResponseType = Response;
};

/// Determine the Request and Response parameter for an RPC based on the Stub
/// signature - mismatch case.
template <typename MemberFunction>
struct CheckAsyncUnaryStreamRpcSignature : public std::false_type {
  using RequestType = int;
  using ResponseType = int;
};

/// Determine the Request and Response parameter for an RPC based on the Stub
/// signature - match case.
template <typename Request, typename Response>
struct CheckAsyncUnaryStreamRpcSignature<
    std::unique_ptr<grpc::ClientAsyncReaderInterface<Response>>(
        grpc::ClientContext*, const Request&, grpc::CompletionQueue*, void*)>
    : public std::true_type {
  using RequestType = Request;
  using ResponseType = Response;
};

/**
 * Tests if @p Functor meets the requirements for a RunAsync callback.
 *
 * @tparam Functor a type the application wants to use as a callback.
 */
template <typename Functor>
using CheckRunAsyncCallback =
    google::cloud::internal::is_invocable<Functor, CompletionQueue&>;

/**
 * The implementation details for `CompletionQueue`.
 *
 * `CompletionQueue` is implemented using the PImpl idiom:
 *     https://en.wikipedia.org/wiki/Opaque_pointer
 * This is the implementation class in that idiom.
 */
class CompletionQueueImpl {
 public:
  CompletionQueueImpl() : cq_(), shutdown_(false) {}
  virtual ~CompletionQueueImpl() = default;

  /**
   * Run the event loop until Shutdown() is called.
   *
   * @param cq the completion queue wrapping this implementation class, used to
   *   notify any asynchronous operation that completes.
   */
  void Run(CompletionQueue& cq);

  /// Terminate the event loop.
  void Shutdown();

  /// Create a new alarm object.
  virtual std::unique_ptr<grpc::Alarm> CreateAlarm() const;

  /// The underlying gRPC completion queue.
  grpc::CompletionQueue& cq() { return cq_; }

  /// Add a new asynchronous operation to the completion queue.
  void* RegisterOperation(std::shared_ptr<AsyncGrpcOperation> op);

 protected:
  /// Return the asynchronous operation associated with @p tag.
  std::shared_ptr<AsyncGrpcOperation> FindOperation(void* tag);

  /// Unregister @p tag from pending operations.
  void ForgetOperation(void* tag);

  /// Simulate a completed operation, provided only to support unit tests.
  void SimulateCompletion(CompletionQueue& cq, AsyncOperation* op, bool ok);

  /// Simulate completion of all pending operations, provided only to support
  /// unit tests.
  void SimulateCompletion(CompletionQueue& cq, bool ok);

  bool empty() const {
    std::unique_lock<std::mutex> lk(mu_);
    return pending_ops_.empty();
  }

  std::size_t size() const {
    std::unique_lock<std::mutex> lk(mu_);
    return pending_ops_.size();
  }

 private:
  grpc::CompletionQueue cq_;
  std::atomic<bool> shutdown_;
  mutable std::mutex mu_;
  std::unordered_map<std::intptr_t, std::shared_ptr<AsyncGrpcOperation>>
      pending_ops_;
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_COMPLETION_QUEUE_IMPL_H_
