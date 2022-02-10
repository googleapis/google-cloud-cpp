// Copyright 2021 Google LLC
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

#include "google/cloud/async_streaming_read_write_rpc.h"
#include "google/cloud/internal/backoff_policy.h"
#include "google/cloud/internal/retry_policy.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <chrono>
#include <memory>

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
   * Return the final status of the streaming RPC.
   *
   * The application must wait until all pending `Read()` and `Write()`
   * operations have completed before calling `Finish()`.
   *
   * @note The future from `Start` will finish before the future from `Finish`.
   */
  virtual future<Status> Finish() = 0;
};

template <typename RequestType, typename ResponseType>
class ResumableAsyncStreamingReadWriteRpcImpl
    : public ResumableAsyncStreamingReadWriteRpc<RequestType, ResponseType> {
 public:
  ResumableAsyncStreamingReadWriteRpcImpl(
      std::shared_ptr<RetryPolicy const> retry_policy,
      std::shared_ptr<BackoffPolicy const> backoff_policy,
      AsyncSleeper const& sleeper,
      AsyncStreamFactory<RequestType, ResponseType> stream_factory,
      StreamInitializer<RequestType, ResponseType> initializer)
      : retry_policy_prototype_(std::move(retry_policy)),
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
      retry_promise_.emplace();
    }

    // RetryPolicy doesn't have a `clone` function
    Initialize(std::make_shared<RetryPolicy const>(*retry_policy_prototype_),
               backoff_policy_prototype_->clone());
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
          read_reinit_done_.emplace();
          return read_reinit_done_->get_future();
        case State::kInitialized:
          read_future = stream_->Read();
          in_progress_read_.emplace();
      }
    }

    return read_future.then(
        [this](future<absl::optional<ResponseType>> optional_response_future) {
          in_progress_read_->set_value();
          {
            std::lock_guard<std::mutex> g{mu_};
            in_progress_read_ = absl::nullopt;
          }
          auto optional_response = optional_response_future.get();
          if (optional_response.has_value()) {
            return make_ready_future(std::move(optional_response));
          }

          ReadWriteRetryFailedStream(std::ref(read_reinit_done_),
                                     std::ref(in_progress_write_));

          return read_reinit_done_->get_future().then([](future<void>) {
            return make_ready_future(absl::optional<ResponseType>());
          });
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
          write_reinit_done_.emplace();
          return write_reinit_done_->get_future().then(
              [](future<void>) { return make_ready_future(false); });
        case State::kInitialized:
          write_future = stream_->Write(r, o);
          in_progress_write_.emplace();
      }
    }

    return write_future.then([this](future<bool> write_fu) {
      in_progress_write_->set_value();
      {
        std::lock_guard<std::mutex> g{mu_};
        in_progress_write_ = absl::nullopt;
      }

      if (write_fu.get()) {
        return make_ready_future(true);
      }

      ReadWriteRetryFailedStream(std::ref(write_reinit_done_),
                                 std::ref(in_progress_read_));

      return write_reinit_done_->get_future().then(
          [](future<void>) { return make_ready_future(false); });
    });
  }

  future<Status> Finish() override {
    {
      std::lock_guard<std::mutex> g{mu_};
      if (stream_state_ == State::kShutdown) {
        return make_ready_future(
            Status(StatusCode::kAborted, "Permanent error"));
      }
      stream_state_ = State::kShutdown;
      if (retry_promise_.has_value()) {
        // can the then occur inline when it's a return value?
        return retry_promise_->get_future().then(
            [this](future<Status> retry_fu) {
              auto status = retry_fu.get();
              if (status.ok()) {
                future<Status> finish_fu;
                {
                  std::lock_guard<std::mutex> g{mu_};
                  finish_fu = stream_->Finish();
                }
                return finish_fu;
              }
              return make_ready_future(status);
            });
      }
    }

    status_promise_.set_value(Status(StatusCode::kOk, "Ok"));
    return stream_->Finish();
  }

 private:
  enum class State { kRetrying, kInitialized, kShutdown };

  // helper uses abstractions, but Dan said that this shouldn't need
  // abstractions
  void ReadWriteRetryFailedStream(
      std::reference_wrapper<absl::optional<promise<void>>> reinit_done,
      std::reference_wrapper<absl::optional<promise<void>>> in_progress_op) {
    bool retrying;

    {
      std::lock_guard<std::mutex> g{mu_};
      reinit_done.get().emplace();
      retrying = stream_state_ == State::kRetrying;
    }

    if (!retrying) {
      bool outstanding_operation;
      future<void> outstanding_operation_future;

      {
        std::lock_guard<std::mutex> g{mu_};
        stream_state_ = State::kRetrying;
        if (!retry_promise_.has_value()) {
          retry_promise_.emplace();
        }

        // If an outstanding operation is present, we can't enter the retry
        // loop, so we defer it until the outstanding `Write` finishes at
        // which point we can enter the retry loop. Since we will return
        // `reinit_done_`, we guarantee that another operation of the same type
        // is not called while we're waiting for the outstanding operation to
        // finish and the retry loop to finish afterward.

        outstanding_operation = in_progress_op.get().has_value();
        if (outstanding_operation) {
          outstanding_operation_future = in_progress_op.get()->get_future();
        }
      }

      if (outstanding_operation) {
        outstanding_operation_future.then(
            [this](future<void>) { FinishOnStreamFail(); });
      } else {
        FinishOnStreamFail();
      }
    }
  }

  void OnFailure(Status const& status) {
    AttemptRetry(status,
                 std::make_shared<RetryPolicy const>(*retry_policy_prototype_),
                 backoff_policy_prototype_->clone());
  }

  void FinishOnStreamFail() {
    future<Status> fail_finish;
    {
      std::lock_guard<std::mutex> g{mu_};
      fail_finish = stream_->Finish();
    }
    fail_finish.then([this](future<Status> finish_status) {
      OnFailure(finish_status.get());
    });
  }

  void SetReadWriteFutures() {
    bool read_pending;
    bool write_pending;

    {
      std::lock_guard<std::mutex> g{mu_};
      read_pending = read_reinit_done_.has_value();
      write_pending = write_reinit_done_.has_value();
    }

    if (read_pending) {
      // want to set member variable to nullopt before setting future returned
      // to caller
      absl::optional<promise<void>> read_promise;
      {
        std::lock_guard<std::mutex> g{mu_};
        read_promise = std::move(read_reinit_done_);
        read_reinit_done_ = absl::nullopt;
      }
      read_promise->set_value();
    }

    if (write_pending) {
      // want to set member variable to nullopt before setting future returned
      // to caller
      absl::optional<promise<void>> write_promise;
      {
        std::lock_guard<std::mutex> g{mu_};
        write_promise = std::move(write_reinit_done_);
        write_reinit_done_ = absl::nullopt;
      }
      write_promise->set_value();
    }
  }

  void AttemptRetry(Status const& status,
                    std::shared_ptr<RetryPolicy> retry_policy,
                    std::shared_ptr<BackoffPolicy> backoff_policy) {
    bool shutdown;
    {
      std::lock_guard<std::mutex> g{mu_};
      shutdown = stream_state_ == State::kShutdown;
    }
    if (!shutdown && !retry_policy->IsExhausted() &&
        retry_policy->OnFailure(status)) {
      sleeper_(backoff_policy->OnCompletion())
          .then([this, retry_policy, backoff_policy](future<void>) {
            Initialize();
          });
      return;
    }

    absl::optional<promise<Status>> retry_promise;
    {
      std::lock_guard<std::mutex> g{mu_};
      stream_state_ = State::kShutdown;
      retry_promise = std::move(retry_promise_);
      retry_promise_ = absl::nullopt;
    }
    retry_promise->set_value(status);
    status_promise_.set_value(status);
    SetReadWriteFutures();
  }

  void Initialize(std::shared_ptr<RetryPolicy> retry_policy,
                  std::shared_ptr<BackoffPolicy> backoff_policy) {
    future<bool> start_future;

    {
      std::lock_guard<std::mutex> g{mu_};
      stream_ = stream_factory_();
      start_future = stream_->Start();
    }

    auto start_future_result =
        start_future.then([this](future<bool> start_future) {
          if (!start_future.get()) {
            future<Status> fail_finish;
            {
              std::lock_guard<std::mutex> g{mu_};
              fail_finish = stream_->Finish();
            }
            return fail_finish.then([this](future<Status> fail_status) {
              return StatusOr<std::unique_ptr<
                  AsyncStreamingReadWriteRpc<RequestType, ResponseType>>>(
                  fail_status.get());
            });
          }
          return initializer_(std::move(stream_));
        });

    start_future_result.then(
        [this, retry_policy, backoff_policy](
            future<StatusOr<std::unique_ptr<
                AsyncStreamingReadWriteRpc<RequestType, ResponseType>>>>
                start_initialize_future) {
          auto start_initialize_response = start_initialize_future.get();

          if (!start_initialize_response.ok()) {
            AttemptRetry(start_initialize_response.status(), retry_policy,
                         backoff_policy);
            return;
          }

          absl::optional<promise<Status>> retry_promise;
          {
            std::lock_guard<std::mutex> g{mu_};
            stream_ = std::move(*start_initialize_response);
            // if `Finish` acquires lock and updates `stream_state_` before
            // this, we don't want `stream_state_` to be changed again
            if (stream_state_ != State::kShutdown) {
              stream_state_ = State::kInitialized;
            }
            retry_promise = std::move(retry_promise_);
            retry_promise_ = absl::nullopt;
          }
          retry_promise->set_value(Status());

          SetReadWriteFutures();
        });
  }

  std::shared_ptr<RetryPolicy const> const retry_policy_prototype_;
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
  absl::optional<promise<Status>> retry_promise_;    // ABSL_GUARDED_BY(mu_)
  promise<Status> status_promise_;                   // ABSL_GUARDED_BY(mu_)
};

/// A helper to apply type deduction.
template <typename RequestType, typename ResponseType>
std::shared_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>>
MakeAsyncResumableStreamingReadWriteRpc(
    std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy, AsyncSleeper sleeper,
    AsyncStreamFactory<ResponseType, RequestType> stream_factory,
    StreamInitializer<ResponseType, RequestType> initializer) {
  return std::make_shared<
      AsyncStreamingReadWriteRpc<ResponseType, RequestType>>(
      std::move(retry_policy), std::move(backoff_policy), sleeper,
      std::move(stream_factory), std::move(initializer));
}

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_RESUMABLE_ASYNC_STREAMING_READ_WRITE_RPC_H
