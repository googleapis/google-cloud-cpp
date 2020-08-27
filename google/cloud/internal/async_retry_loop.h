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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_RETRY_LOOP_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_RETRY_LOOP_H

#include "google/cloud/backoff_policy.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/internal/retry_loop_helpers.h"
#include "google/cloud/internal/retry_policy.h"
#include "google/cloud/version.h"
#include "absl/meta/type_traits.h"
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

template <typename T>
struct FutureValueType;

template <typename T>
struct FutureValueType<future<T>> {
  using value_type = T;
};

/**
 * Implement an asynchronous retry loop for wrapped gRPC requests.
 *
 * In newer libraries the stubs wrap asynchronous RPCs to match this signature:
 *
 * @code
 * class MyStub { public:
 *   virtual future<StatusOr<ResponseProto>> AsyncRpcName(
 *      google::cloud::CompletionQueue& cq,
 *      std::unique_ptr<grpc::ClientContext> context,
 *      RequestProto const& request) = 0;
 * };
 * @endcode
 *
 * stubs with such a signature are easier to mock and test. In most mocks all
 * we need to do is return a future satisfied immediately. And writing the
 * implementation of these stubs is very easy too.
 *
 * This class implements the retry loop for such an RPC.
 */
template <typename Functor, typename Request>
class AsyncRetryLoopImpl : public std::enable_shared_from_this<
                               AsyncRetryLoopImpl<Functor, Request>> {
 public:
  AsyncRetryLoopImpl(std::unique_ptr<RetryPolicy> retry_policy,
                     std::unique_ptr<BackoffPolicy> backoff_policy,
                     bool is_idempotent, google::cloud::CompletionQueue cq,
                     Functor&& functor, Request request, char const* location)
      : retry_policy_(std::move(retry_policy)),
        backoff_policy_(std::move(backoff_policy)),
        is_idempotent_(is_idempotent),
        cq_(std::move(cq)),
        functor_(std::forward<Functor>(functor)),
        request_(std::move(request)),
        location_(location) {}

  using ReturnType = google::cloud::internal::invoke_result_t<
      Functor, google::cloud::CompletionQueue&,
      std::unique_ptr<grpc::ClientContext>, Request const&>;
  using T = typename FutureValueType<ReturnType>::value_type;

  future<T> Start() {
    StartAttempt();
    return result_.get_future();
  }

 private:
  void StartAttempt() {
    if (retry_policy_->IsExhausted()) {
      result_.set_value(
          RetryLoopError("Retry policy exhausted in", location_, last_status_));
      return;
    }
    auto self = this->shared_from_this();
    functor_(cq_, absl::make_unique<grpc::ClientContext>(), request_)
        .then([self](future<T> f) { self->OnAttempt(f.get()); });
  }

  void OnAttempt(T result) {
    // A successful attempt, set the value and finish the loop.
    if (result.ok()) {
      result_.set_value(std::move(result));
      return;
    }
    // Some kind of failure, first verify that it is retryable.
    last_status_ = GetResultStatus(std::move(result));
    if (!is_idempotent_) {
      result_.set_value(RetryLoopError("Error in non-idempotent operation",
                                       location_, last_status_));
      return;
    }
    if (!retry_policy_->OnFailure(last_status_)) {
      if (retry_policy_->IsPermanentFailure(last_status_)) {
        result_.set_value(
            RetryLoopError("Permanent error in", location_, last_status_));
      } else {
        result_.set_value(RetryLoopError("Retry policy exhausted in", location_,
                                         last_status_));
      }
      return;
    }
    auto self = this->shared_from_this();
    cq_.MakeRelativeTimer(backoff_policy_->OnCompletion())
        .then(
            [self](future<StatusOr<std::chrono::system_clock::time_point>> f) {
              self->OnBackoffTimer(f.get());
            });
  }

  void OnBackoffTimer(StatusOr<std::chrono::system_clock::time_point> tp) {
    if (!tp) {
      // some kind of error in the CompletionQueue, probably shutting down.
      result_.set_value(RetryLoopError("Timer failure in", location_,
                                       std::move(tp).status()));
      return;
    }
    StartAttempt();
  }

  std::unique_ptr<RetryPolicy> retry_policy_;
  std::unique_ptr<BackoffPolicy> backoff_policy_;
  bool is_idempotent_ = false;
  google::cloud::CompletionQueue cq_;
  absl::decay_t<Functor> functor_;
  Request request_;
  char const* location_ = "unknown";
  Status last_status_ = Status(StatusCode::kUnknown, "Retry policy exhausted");
  promise<T> result_;
};

/**
 * Create the right AsyncRetryLoopImpl object and start the retry loop on it.
 */
template <typename Functor, typename Request,
          typename std::enable_if<
              google::cloud::internal::is_invocable<
                  Functor, google::cloud::CompletionQueue&,
                  std::unique_ptr<grpc::ClientContext>, Request const&>::value,
              int>::type = 0>
auto AsyncRetryLoop(std::unique_ptr<RetryPolicy> retry_policy,
                    std::unique_ptr<BackoffPolicy> backoff_policy,
                    bool is_idempotent, google::cloud::CompletionQueue cq,
                    Functor&& functor, Request request, char const* location)
    -> google::cloud::internal::invoke_result_t<
        Functor, google::cloud::CompletionQueue&,
        std::unique_ptr<grpc::ClientContext>, Request const&> {
  auto loop = std::make_shared<AsyncRetryLoopImpl<Functor, Request>>(
      std::move(retry_policy), std::move(backoff_policy), is_idempotent,
      std::move(cq), std::forward<Functor>(functor), std::move(request),
      location);
  return loop->Start();
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_RETRY_LOOP_H
