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
#include <thread>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

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

using AsyncSleeper = std::function<future<void>(std::chrono::duration)>;

template <typename RequestType, typename ResponseType>
class ResumableAsyncStreamingReadWriteRpc {
 public:
  virtual ~ResumableAsyncStreamingReadWriteRpc() = default;

  /**
   * Start the streaming RPC.
   *
   * The returned future will be returned with a status
   * when this stream will not be resumed or when the user calls `Finish()`.
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
   * returned. If it is not engaged, the call failed, but the user may call `Read`
   * again unless `Start` had finished with a permanent error `Status`.
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
  virtual future<bool> Write(Request const&, grpc::WriteOptions) = 0;

  /**
   * Return the final status of the streaming RPC.
   *
   * The application must wait until all pending `Read()` and `Write()`
   * operations have completed before calling `Finish()`.
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
      AsyncSleeper const sleeper,
      AsyncStreamFactory<RequestType, ResponseType> stream_factory,
      StreamInitializer<RequestType, ResponseType> initializer)
      : retry_policy_prototype_(std::move(retry_policy)),
        backoff_policy_prototype_(std::move(backoff_policy)),
        sleeper_(std::move(sleeper)),
        stream_factory_(std::move(stream_factory)),
        initializer_(std::move(initializer)) {}

  ResumableAsyncStreamingReadWriteRpcImpl(ResumableStreamingReadRpc&&) = delete;
  ResumableAsyncStreamingReadWriteRpcImpl& operator=(
      ResumableStreamingReadRpc&&) = delete;

  future<Status> Start() override {
    future<Status> status_future;
    {
      std::lock_guard<std::mutex> g{mu_};
      status_future = status_promise_.get_future();
    }
    Initialize(retry_policy_prototype_->clone(),
               backoff_policy_prototype_->clone());
    return status_future;
  }

  future<absl::optional<ResponseType>> Read() override {
    future<absl::optional<ResponseType>> read_future;

    {
      std::lock_guard<std::mutex> g{mu_};
      if (stream_state_ == State::kShutdown) {
        return make_ready_future(absl::optional<ResponseType>());
      }
      if (stream_state_ == State::kRetrying) {
        read_reinit_done_.emplace(absl::make_optional(promise<void>()));
        return read_reinit_done_->get_future();
      }
      read_future = stream_->Read();
      in_progress_read_.emplace(absl::make_optional(promise<void>()));
    }

    return read_future.then([this](future<absl::optional<Response>>
                                       optional_response_future) {
      {
        std::lock_guard<std::mutex> g{mu_};
        in_progress_read_->set_value();
        in_progress_read_ = absl::optional<promise<void>>();
      }
      auto optional_response = optional_response_future.get();
      if (optional_response.has_value()) {
        return make_ready_future(std::move(optional_response));
      }

      bool retrying = false;

      {
        std::lock_guard<std::mutex> g{mu_};
        read_reinit_done_.emplace(absl::make_optional(promise<void>()));
        retrying = stream_state_ == State::kRetrying;
      }

      if (!retrying) {
        bool outstanding_write = false;
        std::unique_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>>
            stream;
        future<void> outstanding_write_future;
        {
          std::lock_guard<std::mutex> g{mu_};
          stream_state_ = State::kRetrying;

          // If an outstanding write is present, we can't enter the retry
          // loop, so we defer it until the outstanding `Write` finishes at
          // which point we can enter the retry loop. Since we will return
          // `read_reinit_done_`, we guarantee that another `Read` is not
          // called while we're waiting for the outstanding write to finish
          // and the retry loop to finish afterward.
          outstanding_write = in_progress_write_.has_value();
          if (outstanding_write) {
            outstanding_write_future = in_progress_write_->get_future();
          }

          // moving will change `stream_` to `nullptr`
          stream = std::move(stream_);
        }

        if (outstanding_write) {
          outstanding_write_future.then(
              [this, stream = std::move(stream)](future<void>) {
                FinishOnStreamFail(std::move(stream));
              });
        } else {
          FinishOnStreamFail(std::move(stream));
        }
      }

      return read_reinit_done_->get_future().then([](future<void>) {
        return make_ready_future(absl::optional<ResponseType>());
      });
    });
  }

  future<bool> Write(Request const& r, grpc::WriteOptions o) override {
    future<bool> write_future;

    {
      std::lock_guard<std::mutex> g{mu_};
      if (stream_state_ == State::kShutdown) {
        return make_ready_future(false);
      }
      if (stream_state_ == State::kRetrying) {
        write_reinit_done_.emplace(absl::make_optional(promise<bool>()));
        return write_reinit_done_->get_future();
      }
      write_future = stream_->Write(r, o);
      in_progress_write_.emplace(absl::make_optional(promise<void>()));
    }

    return write_future.then([this](future<bool> write_future) {
      {
        std::lock_guard<std::mutex> g{mu_};
        in_progress_write_->set_value();
        in_progress_write_ = absl::optional<promise<void>>();
      }

      if (write_fu.get()) {
        return make_ready_future(true);
      }

      bool retrying = false;

      {
        std::lock_guard<std::mutex> g{mu_};
        write_reinit_done_.emplace(absl::make_optional(promise<void>()));
        retrying = stream_state_ == State::kRetrying;
      }

      if (!retrying) {
        bool outstanding_read = false;
        std::unique_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>>
            stream;
        future<void> outstanding_read_future;
        {
          std::lock_guard<std::mutex> g{mu_};
          stream_state_ = State::kRetrying;

          outstanding_read = in_progress_read_.has_value();
          if (outstanding_read) {
            outstanding_read_future = in_progress_read_->get_future();
          }

          stream = std::move(stream_);
        }

        if (outstanding_read) {
          outstanding_read_future.then(
              [this, stream = std::move(stream)](future<void>) {
                FinishOnStreamFail(std::move(stream));
              });
        } else {
          FinishOnStreamFail(std::move(stream));
        }
      }

      return write_reinit_done_->get_future().then(
          [](future<void>) { return make_ready_future(bool); });
    });
  }

  future<Status> Finish() override {
    {
      std::lock_guard<std::mutex> g{mu_};
      if (stream_state_ == State::kShutdown) {
        return make_ready_future(kPermanentErrorStatus);
      }
      stream_state_ = State::kShutdown;
    }

    // Since `Finish` is being called, there must be no outstanding `Read` and
    // `Write`. Along with the status not being `State::kShutdown`, this means
    // that the internal stream is still valid, so we can call `Finish()` on it.
    return stream_->Finish().then([this](future<Status> finish_future) {
      auto finish_response = finish_future.get();
      status_promise_.set_value(finish_response);
      return make_ready_future(finish_response);
    });
  }

 private:
  enum class State { kRetrying, kInitialized, kShutdown };

  void OnFailure(Status status) {
    AttemptRetry(status, retry_policy_prototype_->clone(),
                 backoff_policy_prototype_->clone());
  }

  void FinishOnStreamFail(
      std::unique_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>>
          stream) {
    stream->Finish().then([this](future<Status> finish_status) {
      OnFailure(finish_status.get());
    });
  }

  void SetReadWriteFutures() {
    if (read_reinit_done_.has_value()) {
      // want to set member variable to nullopt before setting future returned
      // to caller
      auto read_promise = read_reinit_done_;
      read_reinit_done_ = absl::optional();
      read_promise->set_value();
    }
    if (write_reinit_done_.has_value()) {
      auto write_promise = write_reinit_done_;
      write_reinit_done_ = absl::optional();
      write_promise->set_value();
    }
  }

  void AttemptRetry(Status status, std::shared_ptr<RetryPolicy> retry_policy,
                    std::shared_ptr<BackoffPolicy> backoff_policy) {
    bool shutdown;
    {
      std::lock_guard<std::mutex> g{mu_};
      shutdown = stream_state_ == State::kShutdown;
    }
    if (!shutdown && !retry_policy->IsExhausted() &&
        retry_policy->OnFailure(status)) {
      sleeper_(backoff_policy->OnCompletion())
          .then([this, retry_policy, backoff_policy](future<void> sleep_fu) {
            Initialize();
          });
      return;
    }

    {
      std::lock_guard<std::mutex> g{mu_};
      stream_state_ = State::kShutdown;
    }
    status_promise_.set_value(kPermanentErrorStatus);
    SetReadWriteFutures();
  }

  void Initialize(std::shared_ptr<RetryPolicy> retry_policy,
                  std::shared_ptr<BackoffPolicy> backoff_policy) {
    std::unique_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>>
        potential_stream;
    future<bool> start_future;

    potential_stream = stream_factory_();
    start_future = potential_stream->Start();

    auto start_future_result = start_future.then(
        [this, potential_stream =
                   std::move(potential_stream)](future<bool> start_future) {
          if (!start_future.get()) {
            {
              std::lock_guard<std::mutex> g{mu_};
              stream_state_ = State::kRetrying;
            }

            FinishOnStreamFail(std::move(potential_stream));

            // return this so the below continuation knows that it failed at the
            // `Start` step
            return make_ready_future(
                StatusOr<std::unique_ptr<
                    AsyncStreamingReadWriteRpc<RequestType, ResponseType>>>(
                    kStartErrorStatus));
          }
          return initializer_(std::move(potential_stream));
        });

    start_future_result.then(
        [this, retry_policy, backoff_policy](
            future<StatusOr<std::unique_ptr<
                AsyncStreamingReadWriteRpc<RequestType, ResponseType>>>>
                start_initialize_future) {
          auto start_initialize_response = start_initialize_future.get();

          if (!start_initialize_response.ok()) {
            // Start failure which already invoked retry loop, so return
            // immediately
            if (start_initialize_response.status() == kStartErrorStatus) {
              return;
            }

            {
              std::lock_guard<std::mutex> g{mu_};
              stream_state_ = State::kRetrying;
            }

            AttemptRetry(kInitializerErrorStatus, retry_policy, backoff_policy);
            return;
          }

          {
            std::lock_guard<std::mutex> g{mu_};
            stream_ = std::move(*start_initialize_response);
            stream_state_ = State::kInitialized;
          }
          SetReadWriteFutures();
        });
  }

  static constexpr kStartErrorStatus =
      Status(StatusCode::kUnavailable, "Start");
  static constexpr kInitializerErrorStatus =
      Status(StatusCode::kUnavailable, "Initializer");
  static constexpr kPermanentErrorStatus =
      Status(StatusCode::kAborted, "Permanent error");

  std::unique_ptr<RetryPolicy const> const retry_policy_prototype_;
  std::unique_ptr<BackoffPolicy const> const backoff_policy_prototype_;
  AsyncSleeper const sleeper_;
  StreamFactory<RequestType, ResponseType> const stream_factory_;
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
}

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_RESUMABLE_ASYNC_STREAMING_READ_WRITE_RPC_H
