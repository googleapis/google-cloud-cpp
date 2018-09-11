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
#include <grpcpp/alarm.h>
#include <grpcpp/support/async_unary_call.h>
#include <atomic>
#include <unordered_map>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
class CompletionQueue;
namespace internal {

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
template <typename Functor>
class AsyncTimerFunctor : public AsyncOperation {
 public:
  explicit AsyncTimerFunctor(Functor&& functor)
      : functor_(std::move(functor)), alarm_(new grpc::Alarm) {}

  void Notify(CompletionQueue& cq, Disposition d) override {
    alarm_.reset();
    functor_(cq, timer_, d);
  }

  void Set(grpc::CompletionQueue& cq,
           std::chrono::system_clock::time_point deadline, void* tag) {
    timer_.deadline = deadline;
    alarm_->Set(&cq, deadline, tag);
  }

  void Cancel() override {
    if (alarm_) {
      alarm_->Cancel();
    }
  }

 private:
  Functor functor_;
  AsyncTimerResult timer_;
  std::unique_ptr<grpc::Alarm> alarm_;
};

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
template <typename Request, typename Response, typename Functor>
class AsyncUnaryRpcFunctor : public AsyncOperation {
 public:
  explicit AsyncUnaryRpcFunctor(Functor&& functor)
      : functor_(std::forward<Functor>(functor)) {}

  void Notify(CompletionQueue& cq, Disposition d) override {
    functor_(cq, result_, d);
  }

  void Cancel() override { result_.context->TryCancel(); }

  /// Make the RPC request and prepare the response callback.
  template <typename Client, typename MemberFunction>
  void Set(Client& client, MemberFunction Client::*call,
           std::unique_ptr<grpc::ClientContext> context, Request const& request,
           grpc::CompletionQueue* cq, void* tag) {
    result_.context = std::move(context);
    auto rpc = (client.*call)(result_.context.get(), request, cq);
    rpc->Finish(&result_.response, &result_.status, tag);
  }

 private:
  Functor functor_;
  AsyncUnaryRpcResult<Response> result_;
};  // namespace internal

/// Determine the Request and Response parameter for an RPC based on the Stub
/// signature - mismatch case.
template <typename MemberFunction>
struct CheckAsyncUnaryRpcSignature : public std::false_type {
  using RequestType = void;
  using ResponseType = void;
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

  /// The underlying gRPC completion queue.
  grpc::CompletionQueue& cq() { return cq_; }

  /// Add a new asynchronous operation to the completion queue.
  void* RegisterOperation(std::shared_ptr<AsyncOperation> op);

  /// Return the asynchronous operation associated with @p tag.
  std::shared_ptr<AsyncOperation> CompletedOperation(void* tag);

 protected:
  /// Simulate a completed operation, provided only to support unit tests.
  void SimulateCompletion(CompletionQueue& cq, AsyncOperation* op,
                          AsyncOperation::Disposition d);

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
  std::unordered_map<std::intptr_t, std::shared_ptr<AsyncOperation>>
      pending_ops_;
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_COMPLETION_QUEUE_IMPL_H_
