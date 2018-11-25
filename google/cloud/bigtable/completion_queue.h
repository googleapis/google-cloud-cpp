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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_COMPLETION_QUEUE_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_COMPLETION_QUEUE_H_

#include "google/cloud/bigtable/internal/completion_queue_impl.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
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
  void Run();

  /// Terminate the completion queue event loop.
  void Shutdown();

  /**
   * Create a timer that fires at @p deadline.
   *
   * @tparam Functor the functor to call when the timer expires and/or it is
   *   canceled.  It must satisfy the `void(AsyncOperation&,bool)` signature.
   * @param deadline when should the timer expire.
   * @param functor the value of the functor.
   * @return an asynchronous operation wrapping the functor and timer, can be
   *   used to cancel the pending timer.
   */
  template <typename Functor,
            typename std::enable_if<
                internal::CheckTimerCallback<Functor>::value, int>::type = 0>
  std::shared_ptr<AsyncOperation> MakeDeadlineTimer(
      std::chrono::system_clock::time_point deadline, Functor&& functor) {
    auto op = std::make_shared<internal::AsyncTimerFunctor<Functor>>(
        std::forward<Functor>(functor), impl_->CreateAlarm());
    void* tag = impl_->RegisterOperation(op);
    op->Set(impl_->cq(), deadline, tag);
    return op;
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
   * @tparam Functor the functor to call when the timer expires and/or it is
   *   canceled.  It must satisfy the `void(AsyncOperation&,bool)` signature.
   * @param duration when should the timer expire relative to the current time.
   * @param functor the value of the functor.
   * @return an asynchronous operation wrapping the functor and timer, can be
   *   used to cancel the pending timer.
   */
  template <typename Rep, typename Period, typename Functor,
            typename std::enable_if<
                internal::CheckTimerCallback<Functor>::value, int>::type = 0>
  std::shared_ptr<AsyncOperation> MakeRelativeTimer(
      std::chrono::duration<Rep, Period> duration, Functor&& functor) {
    auto deadline = std::chrono::system_clock::now() + duration;
    return MakeDeadlineTimer(deadline, std::forward<Functor>(functor));
  }

  /**
   * Make an asynchronous unary RPC.
   *
   * @param client the object implementing the asynchronous API.
   * @param call the pointer to the member function to invoke.
   * @param request the contents of the request.
   * @param context an initialized request context to make the call.
   * @param f the callback to report completion of the call.
   *
   * @tparam Client the type of the class to call.  Typically this would be a
   *   wrapper like DataClient or AdminClient.
   * @tparam MemberFunction the type of the member function in the @p Client
   *     to call. It must meet the requirements for
   *     `internal::CheckAsyncUnaryRpcSignature<>`.
   * @tparam Request the type of the request parameter in the gRPC.
   * @tparam Functor the type of the callback provided by the application. It
   *     must satisfy (using C++17 classes):
   *     @code
   *     static_assert(std::is_invocable_v<
   *         Functor, internal::AsyncUnaryRpc<RequestType,ResponseType>&, bool>)
   *     @endcode
   *
   * @return an AsyncOperation instance that can be used to request cancelation
   *   of the pending operation.
   */
  template <
      typename Client, typename MemberFunction, typename Request,
      typename Functor,
      typename Sig = internal::CheckAsyncUnaryRpcSignature<MemberFunction>,
      typename std::enable_if<Sig::value, int>::type
          valid_member_function_type = 0,
      typename std::enable_if<internal::CheckUnaryRpcCallback<
                                  Functor, typename Sig::ResponseType>::value,
                              int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> MakeUnaryRpc(
      Client& client, MemberFunction Client::*call, Request const& request,
      std::unique_ptr<grpc::ClientContext> context, Functor&& f) {
    static_assert(std::is_same<typename Sig::RequestType,
                               typename std::decay<Request>::type>::value,
                  "Mismatched pointer to member function and request types");
    auto op = std::make_shared<internal::AsyncUnaryRpcFunctor<
        typename Sig::RequestType, typename Sig::ResponseType, Functor>>(
        std::forward<Functor>(f));
    void* tag = impl_->RegisterOperation(op);
    op->Set(client, call, std::move(context), request, &impl_->cq(), tag);
    return op;
  }

  /**
   * Make an asynchronous unary RPC with streamed response.
   *
   * @param client the object implementing the asynchronous API.
   * @param call the pointer to the member function to invoke.
   * @param request the contents of the request.
   * @param context an initialized request context to make the call.
   * @param data_functor the callback to report the arrival of a piece of data.
   * @param finished_functor the callback to report completion of the stream and
   *     its status.
   *
   * @tparam Client the type of the class to call.  Typically this would be a
   *   wrapper like DataClient or AdminClient.
   * @tparam MemberFunction the type of the member function in the @p Client
   *     to call. It must meet the requirements for
   *     `internal::CheckAsyncUnaryStreamRpcSignature<>`.
   * @tparam Request the type of the request parameter in the gRPC.
   * @tparam DataFunctor the type of the callback provided by the application.
   *     It must satisfy (using C++17 classes):
   *     @code
   *     static_assert(std::is_invocable_v<
   *         DataFunctor, CompletionQueue&, const grpc::ClientContext&,
   *         ResponseType&>)
   *     @endcode
   * @tparam FinishedFunctor the type of the callback provided by the
   *     application. It must satisfy (using C++17 classes):
   *     @code
   *     static_assert(std::is_invocable_v<
   *         FinishedFunctor, CompletionQueue&, grpc::ClientContext&,
   *         grpc::Status&>)
   *     @endcode
   *
   * @return an AsyncOperation instance that can be used to request cancelation
   *   of the pending operation.
   */
  template <typename Client, typename MemberFunction, typename Request,
            typename DataFunctor, typename FinishedFunctor,
            typename Sig =
                internal::CheckAsyncUnaryStreamRpcSignature<MemberFunction>,
            typename std::enable_if<Sig::value, int>::type
                valid_member_function_type = 0,
            typename std::enable_if<
                internal::CheckUnaryStreamRpcDataCallback<
                    DataFunctor, typename Sig::ResponseType>::value,
                int>::type valid_data_callback_type = 0,
            typename std::enable_if<
                internal::CheckUnaryStreamRpcFinishedCallback<
                    FinishedFunctor, typename Sig::ResponseType>::value,
                int>::type valid_finished_callback_type = 0>
  std::shared_ptr<AsyncOperation> MakeUnaryStreamRpc(
      Client& client, MemberFunction Client::*call, Request const& request,
      std::unique_ptr<grpc::ClientContext> context, DataFunctor&& data_functor,
      FinishedFunctor&& finished_functor) {
    static_assert(std::is_same<typename Sig::RequestType,
                               typename std::decay<Request>::type>::value,
                  "Mismatched pointer to member function and request types");
    auto op = std::make_shared<internal::AsyncUnaryStreamRpcFunctor<
        typename Sig::RequestType, typename Sig::ResponseType, DataFunctor,
        FinishedFunctor>>(std::forward<DataFunctor>(data_functor),
                          std::forward<FinishedFunctor>(finished_functor));
    void* tag = impl_->RegisterOperation(op);
    op->Set(client, call, std::move(context), request, &impl_->cq(), tag);
    return op;
  }

  /**
   * Asynchronously run a functor on a thread `Run()`ning the `CompletionQueue`.
   *
   * @tparam Functor the functor to call on the CompletionQueue thread.
   *   It must satisfy the `void(CompletionQueue&)` signature.
   * @param functor the value of the functor.
   * @return an asynchronous operation wrapping the functor; it can be used for
   *   cancelling but it makes little sense given that it will be completed
   *   straight away
   */
  template <typename Functor,
            typename std::enable_if<
                internal::CheckRunAsyncCallback<Functor>::value, int>::type = 0>
  std::shared_ptr<AsyncOperation> RunAsync(Functor&& functor) {
    return MakeRelativeTimer(
        std::chrono::seconds(0),
        [functor](CompletionQueue& cq, AsyncTimerResult result) {
          functor(cq);
        });
  }

 private:
  std::shared_ptr<internal::CompletionQueueImpl> impl_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_COMPLETION_QUEUE_H_
