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

using AsyncSleeper = std::function<future<void>(std::chrono::duration<double>)>;

using RetryPolicyFactory = std::function<std::unique_ptr<RetryPolicy>()>;

template <typename RequestType, typename ResponseType>
class ResumableAsyncStreamingReadWriteRpc {
 public:
  virtual ~ResumableAsyncStreamingReadWriteRpc() = default;

  /**
   * Start the streaming RPC.
   *
   * The returned future will be returned with a status
   * when this stream will not be resumed or when the user calls `Finish()`. In
   * the case that there are no errors from `Start`ing the stream and on the
   * latest `Read` and `Write` calls if present, this will return an ok
   * `Status`.
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
   * If the `optional<>` is engaged, a successful `ResponseType` is
   * returned. If it is not engaged, the call failed, but the user may call
   * `Read` again unless `Start` had finished with a permanent error `Status`.
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
   * If `true` is returned, the call was successful. If `false` is returned,
   * the call failed, but the user may call `Write` again unless `Start` had
   * finished with a permanent error `Status`.
   */
  virtual future<bool> Write(RequestType const&, grpc::WriteOptions) = 0;

  /**
   * Finishes the streaming RPC.
   *
   * This will cause any outstanding `Read` or `Write` to fail.
   *
   */
  virtual future<void> Finish() = 0;
};

template <typename RequestType, typename ResponseType>
class ResumableAsyncStreamingReadWriteRpcImpl
    : public ResumableAsyncStreamingReadWriteRpc<RequestType, ResponseType> {
 public:
  ResumableAsyncStreamingReadWriteRpcImpl(
      RetryPolicyFactory retry_factory,
      std::shared_ptr<BackoffPolicy const> backoff_policy,
      std::shared_ptr<AsyncSleeper const> sleeper,
      AsyncStreamFactory<RequestType, ResponseType> stream_factory,
      StreamInitializer<RequestType, ResponseType> initializer)
      : retry_factory_(std::move(retry_factory)),
        backoff_policy_prototype_(std::move(backoff_policy)),
        sleeper_(std::move(sleeper)),
        stream_factory_(std::move(stream_factory)),
        initializer_(std::move(initializer)) {}

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

    Initialize(retry_factory_(), backoff_policy_prototype_->clone());
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
          return read_reinit_done_->get_future().then([](future<void>) {
            return make_ready_future(absl::optional<ResponseType>());
          });
        case State::kInitialized:
          read_future = stream_->Read();
          assert(!in_progress_read_.has_value());
          in_progress_read_.emplace();
      }
    }

    return read_future.then(
        [this](future<absl::optional<ResponseType>> optional_response_future) {
          return OnReadFutureFinish(std::move(optional_response_future));
        });
  }

  future<bool> Write(RequestType const& r, grpc::WriteOptions o) override {
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
              [](future<void>) { return make_ready_future(false); });
        case State::kInitialized:
          write_future = stream_->Write(r, o);
          assert(!in_progress_write_.has_value());
          in_progress_write_.emplace();
      }
    }

    return write_future.then([this](future<bool> write_fu) {
      return OnWriteFutureFinish(std::move(write_fu));
    });
  }

  future<void> Finish() override {
    promise<void> retry_promise;
    future<void> retry_promise_continuation = retry_promise.get_future();
    {
      std::lock_guard<std::mutex> g{mu_};
      switch (stream_state_) {
        case State::kShutdown:
          return make_ready_future();
        case State::kRetrying:
          stream_state_ = State::kShutdown;
          retry_promise_continuation =
              retry_promise_continuation.then(ChainFuture(
                  retry_promise_->get_future().then([this](future<void>) {
                    std::lock_guard<std::mutex> g{mu_};
                    CompleteStreamLockHeld(Status());
                  })));
          break;
        case State::kInitialized:
          stream_state_ = State::kShutdown;
          future<void> root_future = make_ready_future();
          if (in_progress_read_) {
            root_future = root_future.then(
                ChainFuture<void>(in_progress_read_->get_future()));
          }
          if (in_progress_write_) {
            root_future = root_future.then(
                ChainFuture<void>(in_progress_write_->get_future()));
          }
          std::shared_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>>
              stream = std::move(stream_);
          CompleteStreamLockHeld(Status());
          return root_future.then([stream](future<void>) {
            return stream->Finish().then(
                [](future<Status>) { return make_ready_future(); });
          });
      }
    }
    retry_promise.set_value();
    return retry_promise_continuation;
  }

 private:
  using UnderlyingStream =
      std::unique_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>>;

  enum class State { kRetrying, kInitialized, kShutdown };

  future<absl::optional<ResponseType>> OnReadFutureFinish(
      future<absl::optional<ResponseType>> optional_response_future) {
    promise<void> in_progress_read;
    future<void> read_reinit_done;
    auto optional_response = optional_response_future.get();
    bool shutdown;

    {
      std::lock_guard<std::mutex> g{mu_};
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

    return read_reinit_done.then([](future<void>) {
      return make_ready_future(absl::optional<ResponseType>());
    });
  }

  future<bool> OnWriteFutureFinish(future<bool> write_fu) {
    promise<void> in_progress_write;
    future<void> write_reinit_done;
    bool write_response = write_fu.get();
    bool shutdown;

    {
      std::lock_guard<std::mutex> g{mu_};
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

    return write_reinit_done.then(
        [](future<void>) { return make_ready_future(false); });
  }

  void ReadWriteRetryFailedStream() {
    promise<void> root;
    {
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

      if (in_progress_read_.has_value()) {
        assert(!in_progress_write_.has_value());
        root_future = root_future.then(
            ChainFuture<void>(in_progress_read_->get_future()));
      } else if (in_progress_write_.has_value()) {
        root_future = root_future.then(
            ChainFuture<void>(in_progress_write_->get_future()));
      }

      root_future.then([this](future<void>) { FinishOnStreamFail(); });
    }

    root.set_value();
  }

  void FinishOnStreamFail() {
    future<Status> fail_finish;
    {
      std::lock_guard<std::mutex> g{mu_};
      fail_finish = stream_->Finish();
    }
    fail_finish.then([this](future<Status> finish_status) {
      // retry policy refactor
      AttemptRetry(finish_status.get(), retry_factory_(),
                   backoff_policy_prototype_->clone());
    });
  }

  void SetReadWriteFuturesLockHeld() {
    absl::optional<promise<void>> read_reinit_done;
    absl::optional<promise<void>> write_reinit_done;

    {
      read_reinit_done.swap(read_reinit_done_);
      write_reinit_done.swap(write_reinit_done_);
    }
    mu_.unlock();
    if (read_reinit_done) read_reinit_done->set_value();

    if (write_reinit_done) write_reinit_done->set_value();
    mu_.lock();
  }

  void CompleteStreamLockHeld(Status status) {
    SetReadWriteFuturesLockHeld();
    status_promise_.set_value(std::move(status));
  }

  void FinishRetryPromiseLockHeld() {
    promise<void> retry_promise = std::move(*retry_promise_);
    retry_promise_.reset();
    mu_.unlock();
    retry_promise.set_value();
    mu_.lock();
  }

  void AttemptRetry(Status const& status,
                    std::shared_ptr<RetryPolicy> retry_policy,
                    std::shared_ptr<BackoffPolicy> backoff_policy) {
    {
      std::lock_guard<std::mutex> g{mu_};
      if (stream_state_ == State::kShutdown) {
        return FinishRetryPromiseLockHeld();
      }
      assert(stream_state_ == State::kRetrying);
    }

    if (!retry_policy->IsExhausted() && retry_policy->OnFailure(status)) {
      (*sleeper_)(backoff_policy->OnCompletion())
          .then([this, retry_policy, backoff_policy](future<void>) {
            Initialize(retry_policy, backoff_policy);
          });
      return;
    }

    std::lock_guard<std::mutex> g{mu_};
    FinishRetryPromiseLockHeld();
    if (stream_state_ == State::kShutdown) return;
    stream_state_ = State::kShutdown;

    CompleteStreamLockHeld(status);
  }

  void Initialize(std::shared_ptr<RetryPolicy> retry_policy,
                  std::shared_ptr<BackoffPolicy> backoff_policy) {
    future<bool> start_future;

    {
      std::lock_guard<std::mutex> g{mu_};
      if (stream_state_ == State::kShutdown)
        return FinishRetryPromiseLockHeld();
      stream_ = stream_factory_();
      start_future = stream_->Start();
    }

    auto start_future_result =
        start_future.then([this](future<bool> start_future) {
          if (start_future.get()) return make_ready_future(Status());
          std::lock_guard<std::mutex> g{mu_};
          return stream_->Finish();
        });

    auto initializer_future =
        start_future_result.then([this](future<Status> future_status) {
          Status status = future_status.get();
          if (!status.ok())
            return make_ready_future(StatusOr<UnderlyingStream>(status));
          std::lock_guard<std::mutex> g{mu_};
          return initializer_(std::move(stream_));
        });

    initializer_future.then([this, retry_policy,
                             backoff_policy](future<StatusOr<UnderlyingStream>>
                                                 initialize_future) {
      auto start_initialize_response = initialize_future.get();

      if (!start_initialize_response.ok()) {
        AttemptRetry(start_initialize_response.status(), retry_policy,
                     backoff_policy);
        return;
      }

      {
        std::lock_guard<std::mutex> g{mu_};
        if (stream_state_ == State::kShutdown) {
          std::shared_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>>
              to_delete = std::move(*start_initialize_response);
          to_delete->Finish().then([to_delete](future<Status>) {});
        } else {
          stream_ = std::move(*start_initialize_response);
          stream_state_ = State::kInitialized;
        }
        FinishRetryPromiseLockHeld();
        SetReadWriteFuturesLockHeld();
      }
    });
  }

  RetryPolicyFactory retry_factory_;
  std::shared_ptr<BackoffPolicy const> const backoff_policy_prototype_;
  std::shared_ptr<AsyncSleeper const> sleeper_;
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
    std::shared_ptr<BackoffPolicy const> backoff_policy,
    std::shared_ptr<AsyncSleeper const> sleeper,
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
