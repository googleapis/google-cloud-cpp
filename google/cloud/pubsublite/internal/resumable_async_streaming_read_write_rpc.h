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
#include "google/cloud/pubsublite/internal/lifecycle_interface.h"
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
using StreamInitializer = std::function<future<StatusOr<
    std::unique_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>>>>(
    std::unique_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>>)>;

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
class ResumableAsyncStreamingReadWriteRpc : public LifecycleInterface {
 public:
  ~ResumableAsyncStreamingReadWriteRpc() override = default;

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
  virtual future<bool> Write(RequestType const&) = 0;
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
    future<absl::optional<ResponseType>> read_future;

    {
      std::lock_guard<std::mutex> g{mu_};
      switch (stream_state_) {
        case State::kShutdown:
          return make_ready_future(absl::optional<ResponseType>());
        case State::kRetrying:
          assert(!read_reinit_done_.has_value());
          read_reinit_done_.emplace();
          return read_reinit_done_->get_future().then(
              [](future<void>) { return absl::optional<ResponseType>(); });
        case State::kInitialized:
          read_future = stream_->Read();
          assert(!in_progress_read_.has_value());
          in_progress_read_.emplace();
      }
    }

    return read_future.then(
        [this](future<absl::optional<ResponseType>> optional_response_future) {
          return OnReadFutureFinish(optional_response_future.get());
        });
  }

  future<bool> Write(RequestType const& r) override {
    future<bool> write_future;

    {
      std::lock_guard<std::mutex> g{mu_};
      switch (stream_state_) {
        case State::kShutdown:
          return make_ready_future(false);
        case State::kRetrying:
          assert(!write_reinit_done_.has_value());
          write_reinit_done_.emplace();
          return write_reinit_done_->get_future().then(
              [](future<void>) { return false; });
        case State::kInitialized:
          write_future = stream_->Write(r, grpc::WriteOptions());
          assert(!in_progress_write_.has_value());
          in_progress_write_.emplace();
      }
    }

    return write_future.then([this](future<bool> write_fu) {
      return OnWriteFutureFinish(write_fu.get());
    });
  }

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
  future<void> Shutdown() override {
    AsyncRoot root_promise;
    future<void> root_future =
        ConfigureShutdownOrder(root_promise.get_future());
    return root_future;
  }

 private:
  using UnderlyingStream =
      std::unique_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>>;

  enum class State { kRetrying, kInitialized, kShutdown };

  future<absl::optional<ResponseType>> OnReadFutureFinish(
      absl::optional<ResponseType> optional_response) {
    promise<void> in_progress_read(null_promise_t{});
    future<void> read_reinit_done;
    bool shutdown;

    {
      std::lock_guard<std::mutex> g{mu_};
      assert(in_progress_read_.has_value());
      in_progress_read = std::move(*in_progress_read_);
      in_progress_read_.reset();
      shutdown = stream_state_ == State::kShutdown;
      if (!optional_response.has_value() && !shutdown) {
        assert(!read_reinit_done_.has_value());
        read_reinit_done_.emplace();
        read_reinit_done = read_reinit_done_->get_future();
      }
    }

    in_progress_read.set_value();

    if (shutdown) {
      return make_ready_future(absl::optional<ResponseType>());
    }

    if (optional_response.has_value()) {
      return make_ready_future(std::move(optional_response));
    }

    ReadWriteRetryFailedStream();

    return read_reinit_done.then(
        [](future<void>) { return absl::optional<ResponseType>(); });
  }

  future<bool> OnWriteFutureFinish(bool write_response) {
    promise<void> in_progress_write(null_promise_t{});
    future<void> write_reinit_done;
    bool shutdown;

    {
      std::lock_guard<std::mutex> g{mu_};
      assert(in_progress_write_.has_value());
      in_progress_write = std::move(*in_progress_write_);
      in_progress_write_.reset();
      shutdown = stream_state_ == State::kShutdown;
      if (!write_response && !shutdown) {
        assert(!write_reinit_done_.has_value());
        write_reinit_done_.emplace();
        write_reinit_done = write_reinit_done_->get_future();
      }
    }

    in_progress_write.set_value();

    if (shutdown) return make_ready_future(false);

    if (write_response) return make_ready_future(true);

    ReadWriteRetryFailedStream();

    return write_reinit_done.then([](future<void>) { return false; });
  }

  void ReadWriteRetryFailedStream() {
    AsyncRoot root;
    std::lock_guard<std::mutex> g{mu_};
    if (stream_state_ != State::kInitialized) return;

    stream_state_ = State::kRetrying;
    assert(!retry_promise_.has_value());
    retry_promise_.emplace();

    // Assuming that a `Read` fails:
    // If an outstanding operation is present, we can't enter the retry
    // loop, so we defer it until the outstanding `Write` finishes at
    // which point we can enter the retry loop. Since we will return
    // `reinit_done_`, we guarantee that another operation of the same type
    // is not called while we're waiting for the outstanding operation to
    // finish and the retry loop to finish afterward.

    future<void> root_future = root.get_future();

    // at most one of these will be set
    if (in_progress_read_.has_value()) {
      root_future =
          root_future.then(ChainFuture(in_progress_read_->get_future()));
    }
    if (in_progress_write_.has_value()) {
      root_future =
          root_future.then(ChainFuture(in_progress_write_->get_future()));
    }

    root_future.then([this](future<void>) { FinishOnStreamFail(); });
  }

  void FinishOnStreamFail() {
    future<Status> fail_finish;
    {
      std::lock_guard<std::mutex> g{mu_};
      fail_finish = stream_->Finish();
    }
    fail_finish.then([this](future<Status> finish_status) {
      // retry policy refactor
      auto retry_policy = retry_factory_();
      auto backoff_policy = backoff_policy_prototype_->clone();
      AttemptRetry(finish_status.get(), std::move(retry_policy),
                   std::move(backoff_policy));
    });
  }

  future<void> ConfigureShutdownOrder(future<void> root_future) {
    std::unique_lock<std::mutex> lk{mu_};
    switch (stream_state_) {
      case State::kShutdown:
        return make_ready_future();
      case State::kRetrying:
        stream_state_ = State::kShutdown;
        root_future = root_future.then(
            ChainFuture(retry_promise_->get_future().then([this](future<void>) {
              std::unique_lock<std::mutex> lk{mu_};
              CompleteUnsatisfiedOps(Status(), lk);
            })));
        break;
      case State::kInitialized:
        stream_state_ = State::kShutdown;
        stream_->Cancel();
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
        CompleteUnsatisfiedOps(Status(), lk);
        root_future = root_future.then(
            [stream](future<void>) { return future<void>(stream->Finish()); });
    }
    return root_future;
  }

  void SetReadWriteFutures(std::unique_lock<std::mutex>& lk) {
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

  void CompleteUnsatisfiedOps(Status status, std::unique_lock<std::mutex>& lk) {
    SetReadWriteFutures(lk);
    lk.unlock();
    status_promise_.set_value(std::move(status));
    lk.lock();
  }

  void FinishRetryPromise(std::unique_lock<std::mutex>& lk) {
    assert(retry_promise_.has_value());
    promise<void> retry_promise = std::move(retry_promise_.value());
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
        return FinishRetryPromise(lk);
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
    FinishRetryPromise(lk);
    if (stream_state_ == State::kShutdown) return;
    stream_state_ = State::kShutdown;

    CompleteUnsatisfiedOps(status, lk);
  }

  void OnInitialize(StatusOr<UnderlyingStream> start_initialize_response,
                    std::shared_ptr<RetryPolicy> retry_policy,
                    std::shared_ptr<BackoffPolicy> backoff_policy) {
    if (!start_initialize_response.ok()) {
      AttemptRetry(std::move(start_initialize_response.status()), retry_policy,
                   backoff_policy);
      return;
    }
    auto stream = std::move(*start_initialize_response);
    std::unique_lock<std::mutex> lk{mu_};
    if (stream_state_ == State::kShutdown) {
      stream->Finish().then([](future<Status>) {});
    } else {
      stream_ = std::move(stream);
      stream_state_ = State::kInitialized;
    }
    FinishRetryPromise(lk);
    SetReadWriteFutures(lk);
  }

  void Initialize(std::shared_ptr<RetryPolicy> retry_policy,
                  std::shared_ptr<BackoffPolicy> backoff_policy) {
    {
      std::unique_lock<std::mutex> lk{mu_};
      if (stream_state_ == State::kShutdown) {
        return FinishRetryPromise(lk);
      }
    }

    // Since we maintain `stream_` as a `std::unique_ptr<>` as explained below,
    // we have to maintain the potential stream, `stream`, as only a
    // `std::unique_ptr<>` as well. We need to support C++11, so to enable the
    // following lambdas access to `stream`, we temporarily wrap the
    // `std::unique_ptr<>` by a `std::shared_ptr<>`.
    std::shared_ptr<UnderlyingStream> stream =
        std::make_shared<UnderlyingStream>(stream_factory_());
    (*stream)
        ->Start()
        .then([stream](future<bool> start_future) {
          if (start_future.get()) return make_ready_future(Status());
          return (*stream)->Finish();
        })
        .then([this, stream](future<Status> future_status) {
          Status status = future_status.get();
          if (!status.ok()) {
            return make_ready_future(StatusOr<UnderlyingStream>(status));
          }
          return initializer_(std::move(*stream));
        })
        .then([this, retry_policy, backoff_policy](
                  future<StatusOr<UnderlyingStream>> initialize_future) {
          OnInitialize(initialize_future.get(), retry_policy, backoff_policy);
        });
  }

  RetryPolicyFactory retry_factory_;
  std::shared_ptr<BackoffPolicy const> const backoff_policy_prototype_;
  AsyncSleeper const sleeper_;
  AsyncStreamFactory<RequestType, ResponseType> const stream_factory_;
  StreamInitializer<RequestType, ResponseType> const initializer_;

  std::mutex mu_;

  // We maintain `stream_` as a `std::unique_ptr<>` so that we ensure that we
  // are the sole owner of the stream. This is important to ensure that we
  // maintain the constraints of the stream's API, ex. we have knowledge of all
  // outstanding operations of `stream_`. A `std::shared_ptr<>` would allow the
  // stream to be leaked through `initializer_`, preventing us from having that
  // certainty.
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
