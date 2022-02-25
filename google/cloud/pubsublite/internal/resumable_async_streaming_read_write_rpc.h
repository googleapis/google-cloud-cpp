// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_RESUMABLE_ASYNC_STREAMING_READ_WRITE_RPC_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_RESUMABLE_ASYNC_STREAMING_READ_WRITE_RPC_H

#include "google/cloud/pubsublite/internal/futures.h"
#include "google/cloud/async_streaming_read_write_rpc.h"
#include "google/cloud/internal/backoff_policy.h"
#include "google/cloud/internal/retry_policy.h"
#include "google/cloud/log.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <chrono>
#include <memory>
#include <utility>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

using google::cloud::internal::BackoffPolicy;
using google::cloud::internal::RetryPolicy;

/**
 * `ResumableAsyncStreamingReadWriteRpc<ResponseType, RequestType>` uses
 * callables compatible with this `std::function<>` to create new streams.
 */
template <typename RequestType, typename ResponseType>
using AsyncStreamFactory = std::function<
    std::unique_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>>()>;

/**
 * `ResumableAsyncStreamingReadWriteRpc<ResponseType, RequestType>` uses
 * callables compatible with this `std::function<>` to initialize a stream
 * from AsyncStreamFactory.
 */
template <typename RequestType, typename ResponseType>
using StreamInitializer = std::function<future<StatusOr<std::shared_ptr<
    std::unique_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>>>>>(
    std::shared_ptr<std::unique_ptr<
        AsyncStreamingReadWriteRpc<RequestType, ResponseType>>>)>;

using AsyncSleeper = std::function<future<void>(std::chrono::milliseconds)>;

using RetryPolicyFactory = std::function<std::unique_ptr<RetryPolicy>()>;

/**
 * Defines the interface for resumable bidirectional streaming RPCs.
 *
 * Concrete instances of this class resume interrupted streaming RPCs after
 * transient failures. On such failures the concrete implementations would
 * typically create a new streaming RPC and call an asynchronous function to
 * to initialize the stream.
 *
 * While resuming a streaming RPC is automatic, callers of `Read` and `Write`
 * are notified when a new stream is created, as they may need to take action
 * when starting on a new stream.
 *
 * Example (sort of unrealistic) usage:
 * @code
 * using Underlying = std::unique_ptr<StreamingReadWriteRpc<Req, Res>>;
 *
 * // Initializes a non-resumable stream, potentially making many
 * // chained asynchronous calls.
 * future<StatusOr<Underlying>> Initialize(Underlying to_init);
 *
 * Status Example() {
 *   std::unique_ptr<ResumableAsyncStreamingReadWriteRpc<Req, Res>> stream =
 *      MakeResumableStream(&Initialize);
 *   future<Status> final_status = stream->Start(); // 1
 *   while (!final_status.is_ready()) {
 *     if (!stream->Write(GetMessage1()).get()) continue;
 *     if (!stream->Write(GetMessage2()).get()) continue;
 *     auto response_1 = stream->Read().get();
 *     if (!response_1.has_value()) continue;
 *     ProcessResponse(*response_1);
 *     auto response_2 = stream->Read().get();
 *     if (!response_2.has_value()) continue;
 *     ProcessResponse(*response_2);
 *     resumable_stream.Shutdown().get();
 *     return Status();
 *   }
 *   return final_status.get();
 * }
 * @endcode
 */
template <typename RequestType, typename ResponseType>
class ResumableAsyncStreamingReadWriteRpc {
 public:
  virtual ~ResumableAsyncStreamingReadWriteRpc() = default;

  /**
   * Start the streaming RPC.
   *
   * The future returned by this function is satisfied when the stream
   * is successfully shut down (in which case in contains an ok status),
   * or when the retry policies to resume the stream are exhausted. The
   * latter includes the case where the stream fails with a permanent
   * error.
   *
   * While the stream is usable immediately after this function returns,
   * any `Read()` or `Write()` calls will fail until the stream is initialized
   * successfully.
   */
  virtual future<Status> Start() = 0;

  /**
   * Read one response from the streaming RPC.
   *
   * @note Only **one** `Read()` operation may be pending at a time. The
   * application is responsible for waiting until any previous `Read()`
   * operations have completed before calling `Read()` again.
   *
   * Whether `Read()` can be called before a `Write()` operation is specified by
   * each service and RPC. Most services require at least one `Write()` call
   * before calling `Read()`. Many services may return more than one response
   * for a single `Write()` request.  Each service and RPC specifies how to
   * discover if more responses will be forthcoming.
   *
   * The future returned by `Read` will be satisfied when the `Read` call on the
   * underlying stream successfully completes or when the internal retry loop
   * (un)successfully completes if the underlying call to `Read` fails.
   *
   * If the future is satisfied with an engaged `optional<>`, it holds a value
   * read from the current underlying GRPC stream. If the future is satisfied
   * with `nullopt`, the underlying stream may have changed or a permanent error
   * has happened. If the `Start` future is not satisfied, the user may call
   * `Read` again to read from a new underlying stream.
   */
  virtual future<absl::optional<ResponseType>> Read() = 0;

  /**
   * Write one request to the streaming RPC.
   *
   * @note Only **one** `Write()` operation may be pending at a time. The
   * application is responsible for waiting until any previous `Write()`
   * operations have completed before calling `Write()` again.
   *
   * Whether `Write()` can be called before waiting for a matching `Read()`
   * operation is specified by each service and RPC. Many services tolerate
   * multiple `Write()` calls before performing or at least receiving a `Read()`
   * response.
   *
   * The future returned by `Write` will be satisfied when the `Write` call on
   * the underlying stream successfully completes or when the internal retry
   * loop (un)successfully completes if the underlying call to `Write` fails.
   *
   * If the future is satisfied with `true`, a successful `Write` call was made
   * to the current underlying GRPC stream. If the future is satisfied with
   * `false`, the underlying stream may have changed or a permanent error has
   * happened. If the `Start` future is not satisfied, the user may call `Write`
   * again to write the value to a new underlying stream.
   */
  virtual future<bool> Write(RequestType const&, grpc::WriteOptions) = 0;

  /**
   * Finishes the streaming RPC.
   *
   * This will cause any outstanding `Read` or `Write` to fail. This may be
   * called while a `Read` or `Write` of an object of this class is outstanding.
   * Internally, the class will manage waiting on `Read` and `Write` calls on a
   * gRPC stream before calling `Finish` on its underlying stream as per
   * `google::cloud::AsyncStreamingReadWriteRpc`. If the class is currently in a
   * retry loop, this will terminate the retry loop and then satisfy the
   * returned future. If the class has a present internal outstanding `Read` or
   * `Write`, this call will satisfy the returned future only after the internal
   * `Read` and/or `Write` finish.
   */
  virtual future<void> Shutdown() = 0;
};

template <typename RequestType, typename ResponseType>
class ResumableAsyncStreamingReadWriteRpcImpl
    : public ResumableAsyncStreamingReadWriteRpc<RequestType, ResponseType> {
 public:
  ResumableAsyncStreamingReadWriteRpcImpl(
      RetryPolicyFactory retry_factory,
      std::shared_ptr<BackoffPolicy const> backoff_policy, AsyncSleeper sleeper,
      AsyncStreamFactory<RequestType, ResponseType> stream_factory,
      StreamInitializer<RequestType, ResponseType> initializer)
      : retry_factory_(std::move(retry_factory)),
        backoff_policy_prototype_(std::move(backoff_policy)),
        sleeper_(std::move(sleeper)),
        stream_factory_(std::move(stream_factory)),
        initializer_(std::move(initializer)) {}

  ~ResumableAsyncStreamingReadWriteRpcImpl() override {
    future<void> shutdown = Shutdown();
    if (!shutdown.is_ready()) {
      GCP_LOG(WARNING) << "`Finish` must be called and finished before object "
                          "goes out of scope.";
      assert(false);
    }
    shutdown.get();
  }

  ResumableAsyncStreamingReadWriteRpcImpl(
      ResumableAsyncStreamingReadWriteRpc<RequestType, ResponseType>&&) =
      delete;
  ResumableAsyncStreamingReadWriteRpcImpl& operator=(
      ResumableAsyncStreamingReadWriteRpc<RequestType, ResponseType>&&) =
      delete;

  future<Status> Start() override {
    future<Status> status_future;
    {
      std::lock_guard<std::mutex> g{mu_};
      status_future = status_promise_.get_future();
      assert(!retry_promise_.has_value());
      retry_promise_.emplace();
    }
    auto retry_policy = retry_factory_();
    auto backoff_policy = backoff_policy_prototype_->clone();
    Initialize(std::move(retry_policy), std::move(backoff_policy));
    return status_future;
  }

  future<absl::optional<ResponseType>> Read() override {
    // TODO(18suresha) implement
    return make_ready_future(absl::optional<ResponseType>());
  }

  future<bool> Write(RequestType const&, grpc::WriteOptions) override {
    // TODO(18suresha) implement
    return make_ready_future(false);
  }

  future<void> Shutdown() override {
    promise<void> root_promise;
    future<void> root_future =
        ConfigureShutdownOrder(root_promise.get_future());

    root_promise.set_value();
    return root_future;
  }

 private:
  using UnderlyingStream =
      std::unique_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>>;

  enum class State { kRetrying, kInitialized, kShutdown };

  future<void> ConfigureShutdownOrder(future<void> root_future) {
    {
      std::unique_lock<std::mutex> lk{mu_};
      switch (stream_state_) {
        case State::kShutdown:
          return make_ready_future();
        case State::kRetrying:
          stream_state_ = State::kShutdown;
          root_future = root_future.then(ChainFuture(
              retry_promise_->get_future().then([this](future<void>) {
                std::unique_lock<std::mutex> lk{mu_};
                CompleteOutstandingOpsLockHeld(Status(), lk);
              })));
          break;
        case State::kInitialized:
          stream_state_ = State::kShutdown;
          if (in_progress_read_) {
            root_future =
                root_future.then(ChainFuture(in_progress_read_->get_future()));
          }
          if (in_progress_write_) {
            root_future =
                root_future.then(ChainFuture(in_progress_write_->get_future()));
          }
          std::shared_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>>
              stream = std::move(stream_);
          CompleteOutstandingOpsLockHeld(Status(), lk);
          root_future = root_future.then([stream](future<void>) {
            return future<void>(stream->Finish());
          });
      }
    }
    return root_future;
  }

  void SetReadWriteFuturesLockHeld(std::unique_lock<std::mutex>& lk) {
    absl::optional<promise<void>> read_reinit_done;
    absl::optional<promise<void>> write_reinit_done;

    {
      read_reinit_done.swap(read_reinit_done_);
      write_reinit_done.swap(write_reinit_done_);
    }
    lk.unlock();
    if (read_reinit_done) read_reinit_done->set_value();

    if (write_reinit_done) write_reinit_done->set_value();
    lk.lock();
  }

  void CompleteOutstandingOpsLockHeld(Status status,
                                      std::unique_lock<std::mutex>& lk) {
    SetReadWriteFuturesLockHeld(lk);
    status_promise_.set_value(std::move(status));
  }

  void FinishRetryPromiseLockHeld(std::unique_lock<std::mutex>& lk) {
    assert(retry_promise_.has_value());
    promise<void> retry_promise = std::move(*retry_promise_);
    retry_promise_.reset();
    lk.unlock();
    retry_promise.set_value();
    lk.lock();
  }

  void AttemptRetry(Status const& status,
                    std::shared_ptr<RetryPolicy> retry_policy,
                    std::shared_ptr<BackoffPolicy> backoff_policy) {
    {
      std::unique_lock<std::mutex> lk{mu_};
      if (stream_state_ == State::kShutdown) {
        return FinishRetryPromiseLockHeld(lk);
      }
      assert(stream_state_ == State::kRetrying);
    }

    if (!retry_policy->IsExhausted() && retry_policy->OnFailure(status)) {
      sleeper_(backoff_policy->OnCompletion())
          .then([this, retry_policy, backoff_policy](future<void>) {
            Initialize(retry_policy, backoff_policy);
          });
      return;
    }

    std::unique_lock<std::mutex> lk{mu_};
    FinishRetryPromiseLockHeld(lk);
    if (stream_state_ == State::kShutdown) return;
    stream_state_ = State::kShutdown;

    CompleteOutstandingOpsLockHeld(status, lk);
  }

  void OnInitialize(
      StatusOr<std::shared_ptr<UnderlyingStream>> start_initialize_response,
      std::shared_ptr<UnderlyingStream> stream,
      std::shared_ptr<RetryPolicy> retry_policy,
      std::shared_ptr<BackoffPolicy> backoff_policy) {
    if (!start_initialize_response.ok()) {
      AttemptRetry(std::move(start_initialize_response.status()), retry_policy,
                   backoff_policy);
      return;
    }

    {
      std::unique_lock<std::mutex> lk{mu_};
      if (stream_state_ == State::kShutdown) {
        (*stream)->Finish().then([](future<Status>) {});
      } else {
        stream_ = std::move(**start_initialize_response);
        stream_state_ = State::kInitialized;
      }
      FinishRetryPromiseLockHeld(lk);
      SetReadWriteFuturesLockHeld(lk);
    }
  }

  void Initialize(std::shared_ptr<RetryPolicy> retry_policy,
                  std::shared_ptr<BackoffPolicy> backoff_policy) {
    future<bool> start_future;
    std::shared_ptr<UnderlyingStream> stream;
    {
      std::unique_lock<std::mutex> lk{mu_};
      if (stream_state_ == State::kShutdown) {
        return FinishRetryPromiseLockHeld(lk);
      }
      stream = std::make_shared<UnderlyingStream>(stream_factory_());
      start_future = (*stream)->Start();
    }

    auto start_future_result =
        start_future.then([this, stream](future<bool> start_future) {
          if (start_future.get()) return make_ready_future(Status());
          std::lock_guard<std::mutex> lk{mu_};
          return (*stream)->Finish();
        });

    auto initializer_future =
        start_future_result.then([this, stream](future<Status> future_status) {
          Status status = future_status.get();
          if (!status.ok()) {
            return make_ready_future(
                StatusOr<std::shared_ptr<UnderlyingStream>>(status));
          }
          std::lock_guard<std::mutex> lk{mu_};
          return initializer_(stream);
        });

    initializer_future.then(
        [this, stream, retry_policy,
         backoff_policy](future<StatusOr<std::shared_ptr<UnderlyingStream>>>
                             initialize_future) {
          OnInitialize(initialize_future.get(), stream, retry_policy,
                       backoff_policy);
        });
  }

  RetryPolicyFactory retry_factory_;
  std::shared_ptr<BackoffPolicy const> const backoff_policy_prototype_;
  AsyncSleeper const sleeper_;
  AsyncStreamFactory<RequestType, ResponseType> const stream_factory_;
  StreamInitializer<RequestType, ResponseType> const initializer_;

  std::mutex mu_;

  std::unique_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>>
      stream_;                             // ABSL_GUARDED_BY(mu_)
  State stream_state_ = State::kRetrying;  // ABSL_GUARDED_BY(mu_)
  // The below two member variables are to present a future to the user when
  // `Read` or `Write` finish with a failure. The returned future is only
  // completed when the invoked retry loop completes on success or permanent
  // error.
  absl::optional<promise<void>> read_reinit_done_;   // ABSL_GUARDED_BY(mu_)
  absl::optional<promise<void>> write_reinit_done_;  // ABSL_GUARDED_BY(mu_)
  // The below two member variables are promises that finish their future when
  // an internal `Read` or `Write` is finished respectively. This allows us to
  // perform retry logic (calling `Finish` internally) right when there are no
  // more outstanding `Read`s or `Write`s.
  absl::optional<promise<void>> in_progress_read_;   // ABSL_GUARDED_BY(mu_)
  absl::optional<promise<void>> in_progress_write_;  // ABSL_GUARDED_BY(mu_)
  absl::optional<promise<void>> retry_promise_;      // ABSL_GUARDED_BY(mu_)

  promise<Status> status_promise_;
};

template <typename RequestType, typename ResponseType>
std::unique_ptr<ResumableAsyncStreamingReadWriteRpc<RequestType, ResponseType>>
MakeResumableAsyncStreamingReadWriteRpcImpl(
    RetryPolicyFactory retry_factory,
    std::shared_ptr<BackoffPolicy const> backoff_policy, AsyncSleeper sleeper,
    AsyncStreamFactory<RequestType, ResponseType> stream_factory,
    StreamInitializer<RequestType, ResponseType> initializer) {
  return absl::make_unique<
      ResumableAsyncStreamingReadWriteRpcImpl<RequestType, ResponseType>>(
      std::move(retry_factory), std::move(backoff_policy), std::move(sleeper),
      std::move(stream_factory), std::move(initializer));
}

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_RESUMABLE_ASYNC_STREAMING_READ_WRITE_RPC_H
