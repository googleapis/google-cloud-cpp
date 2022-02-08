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
   * before calling `Read()`.  Many services may return more than one response
   * for a single `Write()` request.  Each service and RPC specifies how to
   * discover if more responses will be forthcoming.
   *
   * If the `optional<>` is not engaged, a successful `ResponseType` is
   * returned. If it is engaged, the call failed, but the user may call `Read`
   * again unless `Start` had finished with a permanent error `Status`.   The
   * application should wait until any other pending operations (typically any
   * other `Write()` calls) complete and then call `Finish()` to find the status
   * of the streaming RPC.
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
   * finished with a permanent error `Status`. The application should wait until
   * any other pending operations (typically any other `Read()` calls) complete
   * and then call `Finish()` to find the status of the streaming RPC.
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
      if (in_retry_loop_) {
        read_promise_ =
            absl::make_optional(promise<absl::optional<ResponseType>>());
        return read_promise_.value().get_future();
      }
      read_future = stream_->Read();
    }

    return read_future.then(
        [this](future<absl::optional<Response>> optional_response_future) {
          auto optional_response = optional_response_future.get();
          if (optional_response.has_value()) {
            return make_ready_future(optional_response);
          }
          std::lock_guard<std::mutex> g{this->mu_};
          read_promise_ =
              absl::make_optional(promise<absl::optional<ResponseType>>());
          if (!this->in_retry_loop_) {
            this->OnFailure(Status(StatusCode.kUnavailable, "Read"));
            this->in_retry_loop_ = true;
          }
          return read_promise_.value().get_future();
        });
  }

  future<bool> Write(Request const& r, grpc::WriteOptions o) override {
    future<bool> write_future;

    {
      std::lock_guard<std::mutex> g{mu_};
      if (in_retry_loop_) {
        write_promise_ = absl::make_optional(promise<bool>());
        return write_promise_.value().get_future();
      }
      write_future = stream_->Write(r, o);
    }

    return write_future.then([this](future<bool> write_fu) {
      if (write_fu.get()) {
        return make_ready_future(true);
      }
      std::lock_guard<std::mutex> g{this->mu_};
      write_promise_ = absl::make_optional(promise<bool>());
      if (!this->in_retry_loop_) {
        this->OnFailure(Status(StatusCode.kUnavailable, "Write"));
        this->in_retry_loop_ = true;
      }
      return write_promise_.value().get_future();
    });
  }

  future<Status> Finish() override {
    return stream_->Finish().then([this](future<Status> finish_future) {
      auto finish_response = finish_future.get();
      std::lock_guard<std::mutex> g{this->mu_};
      this->status_promise_.set_value(finish_response);
      shutdown_ = true;
      return make_ready_future(finish_response);
    });
  }

 private:
  void OnFailure(Status status) {
    {
      std::lock_guard<std::mutex> g{mu_};
      stream_ = nullptr;
    }
    AttemptRetry(status, retry_policy_prototype_->clone(),
                 backoff_policy_prototype_->clone());
  }

  void AttemptRetry(Status status, std::shared_ptr<RetryPolicy> retry_policy,
                    std::shared_ptr<BackoffPolicy> backoff_policy) {
    bool shutdown;
    {
      std::lock_guard<std::mutex> g{mu_};
      shutdown_ = shutdown;
    }
    if (!shutdown && !retry_policy->IsExhausted() &&
        retry_policy->OnFailure(status)) {
      sleeper_(backoff_policy->OnCompletion())
          .then([this, retry_policy, backoff_policy](future<void> sleep_fu) {
            this->Initialize();
          });
      return;
    }
    std::lock_guard<std::mutex> g{mu_};
    status_promise_.set_value(Status(StatusCode.kAborted, "Permanent error"));
    in_retry_loop_ = false;
  }

  void Initialize(std::shared_ptr<RetryPolicy> retry_policy,
                  std::shared_ptr<BackoffPolicy> backoff_policy) {
    std::unique_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>>
        potential_stream;
    future<bool> start_future;

    potential_stream = stream_factory_();
    start_future = potential_stream->Start();

    start_future.then([this, potential_stream = std::move(potential_stream)](future<bool> start_future) {
      if (!start_future.get()) {
        this->AttemptRetry(Status(StatusCode.kUnavailable, "Start"),
                           retry_policy, backoff_policy);
        return;
      }
      this->initializer_(std::move(potential_stream))
          .then([this, retry_policy, backoff_policy](
                    future<StatusOr<std::unique_ptr<
                        AsyncStreamingReadWriteRpc<RequestType, ResponseType>>>>
                        initialize_future) {
            auto initialize_response = initialize_future.get();
            if (!initialize_response.ok()) {
              this->AttemptRetry(Status(StatusCode.kUnavailable, "Initialize"),
                                 retry_policy, backoff_policy);
              return;
            }
            std::lock_guard<std::mutex> g{this->mu_};
            this->stream_ = std::move(*initialize_response);
            if (this->read_promise_.has_value()) {
              this->read_promise_.value().set_value(absl::optional());
              this->read_promise_ = absl::optional();
            }
            if (this->write_promise_.has_value()) {
              this->write_promise_.value().set_value(false);
              this->write_promise_ = absl::optional();
            }
            this->in_retry_loop_ = false;
          });
    });
  }

  std::unique_ptr<RetryPolicy const> const retry_policy_prototype_;
  std::unique_ptr<BackoffPolicy const> const backoff_policy_prototype_;
  AsyncSleeper sleeper_;
  StreamFactory<RequestType, ResponseType> const stream_factory_;
  StreamInitializer<RequestType, ResponseType> const initializer_;

  std::mutex mu_;

  std::unique_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>>
      stream_;          // ABSL_GUARDED_BY(mu_)
  bool in_retry_loop_;  // ABSL_GUARDED_BY(mu_)
  absl::optional<promise<absl::optional<ResponseType>>>
      read_promise_;                             // ABSL_GUARDED_BY(mu_)
  absl::optional<promise<bool>> write_promise_;  // ABSL_GUARDED_BY(mu_)
  promise<Status> status_promise_;               // ABSL_GUARDED_BY(mu_)
  bool shutdown_ = false;                        // ABSL_GUARDED_BY(mu_)
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
