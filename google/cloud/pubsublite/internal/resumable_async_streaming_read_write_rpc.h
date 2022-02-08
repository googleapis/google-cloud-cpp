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
namespace internal {

/**
 * `ResumableAsyncStreamingReadWriteRpc<ResponseType, RequestType>` uses
 * callables compatible with this `std::function<>` to create new streams.
 */
template <typename ResponseType, typename RequestType>
using AsyncStreamFactory = std::function<
    std::unique_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>>()>;

/**
 * `ResumableAsyncStreamingReadWriteRpc<ResponseType, RequestType>` uses
 * callables compatible with this `std::function<>` to reinitialize a stream
 * from AsyncStreamFactory after the previous stream was broken.
 */
template <typename ResponseType, typename RequestType>
using StreamReinitializer = std::function<future<StatusOr<
    std::unique_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>>>>(
    std::unique_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>>)>;

using AsyncSleeper = std::function<future<void>(std::chrono::duration)>;

template <typename Request, typename Response>
class ResumableAsyncStreamingReadWriteRpc {
 public:
  virtual ~ResumableAsyncStreamingReadWriteRpc() = default;

  /**
   * Start the streaming RPC.
   *
   * The returned future will be returned with a status
   * when this stream will not be resumed.
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
   * If the `optional<>` is not engaged the streaming RPC has completed.  The
   * application should wait until any other pending operations (typically any
   * other `Write()` calls) complete and then call `Finish()` to find the status
   * of the streaming RPC.
   */
  virtual future<absl::optional<Response>> Read() = 0;

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
   * If `Write()` completes with `false` the streaming RPC has completed.   The
   * application should wait until any other pending operations (typically any
   * other `Read()` calls) complete and then call `Finish()` to find the status
   * of the streaming RPC.
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

template <typename ResponseType, typename RequestType>
class ResumableAsyncStreamingReadWriteRpcImpl
    : public ResumableAsyncStreamingReadWriteRpc<RequestType, ResponseType> {
 public:
  ResumableAsyncStreamingReadWriteRpcImpl(
      std::shared_ptr<RetryPolicy const> retry_policy,
      std::shared_ptr<BackoffPolicy const> backoff_policy,
      AsyncSleeper const sleeper,
      AsyncStreamFactory<ResponseType, RequestType> stream_factory,
      StreamReinitializer<ResponseType, RequestType> reinitializer)
      : retry_policy_prototype_(std::move(retry_policy)),
        backoff_policy_prototype_(std::move(backoff_policy)),
        sleeper_(std::move(sleeper)),
        stream_factory_(std::move(stream_factory)),
        reinitializer_(std::move(reinitializer)),
        impl_(stream_factory_()) {}

  ResumableAsyncStreamingReadWriteRpcImpl(ResumableStreamingReadRpc&&) = delete;
  ResumableAsyncStreamingReadWriteRpcImpl& operator=(
      ResumableStreamingReadRpc&&) = delete;

  future<Status> Start() override {
    status_mu_.lock();
    status_promise_ = promise<Status>();
    auto ret = status_promise_.get_future();
    status_mu_.unlock();
    mu_.lock();
    auto fu = impl_->Start();
    mu_.unlock();
    fu.then([this](future<bool> start_fu) {
      if (!start_fu.get()) {
        std::lock_guard(this->retry_loop_mu_);
        if (!this->in_retry_loop_) {
          this->RetryLoop(this->retry_policy_prototype_->clone(),
                          this->backoff_policy_prototype_->clone(),
                          RetryType.Start);
          this->in_retry_loop_ = true;
        }
      }
    });
    return ret;
  }

  future<absl::optional<Response>> Read() override {
    retry_loop_mu_.lock();
    if (in_retry_loop_) {
      read_promise_ =
          absl::make_optional(promise<absl::optional<ResponseType>>());
      retry_loop_mu_.unlock();
      return read_promise_.value().get_future();
    }

    mu_.lock();
    auto fu = impl_->Read();
    mu_.unlock();
    retry_loop_mu_.unlock();

    return fu.then(
        [this](future<absl::optional<Response>> optional_response_future) {
          auto optional_response = optional_response_future.get();
          if (optional_response.has_value()) {
            return make_ready_future(optional_response);
          }
          std::lock_guard(this->retry_loop_mu_);
          read_promise_ =
              absl::make_optional(promise<absl::optional<ResponseType>>());
          if (!this->in_retry_loop_) {
            this->RetryLoop(this->retry_policy_prototype_->clone(),
                            this->backoff_policy_prototype_->clone(),
                            RetryType.Read);
            this->in_retry_loop_ = true;
          }
          return read_promise_.value().get_future();
        });
  }

  future<bool> Write(Request const& r, grpc::WriteOptions o) override {
    retry_loop_mu_.lock();
    if (in_retry_loop_) {
      write_promise_ = absl::make_optional(promise<bool>());
      retry_loop_mu_.unlock();
      return write_promise_.value().get_future();
    }

    mu_.lock();
    auto fu = impl_->Write(r, o);
    mu_.unlock();
    retry_loop_mu_.unlock();

    return fu.then([this](future<bool> write_fu) {
      if (write_fu.get()) {
        return make_ready_future(true);
      }
      std::lock_guard(this->retry_loop_mu_);
      write_promise_ = absl::make_optional(promise<bool>());
      if (!this->in_retry_loop_) {
        this->RetryLoop(this->retry_policy_prototype_->clone(),
                        this->backoff_policy_prototype_->clone(),
                        RetryType.Write);
        this->in_retry_loop_ = true;
      }
      return write_promise_.value().get_future();
    });
  }

  future<Status> Finish() override {
    status_mu_.lock();
    status_promise_.set_value(
        Status(StatusCode.kOk, "Finish() called on stream"));
    status_mu_.unlock();
    std::lock_guard<std::mutex> g{mu_};
    return impl_->Finish();
  }

 private:
  enum class RetryType { Start, Read, Write };

  void RetryLoop(std::shared_ptr<RetryPolicy> retry_policy,
                 std::shared_ptr<BackoffPolicy> backoff_policy,
                 RetryType retry_type) {
    if (!retry_policy->IsExhausted()) {
      this->sleeper_(backoff_policy->OnCompletion())
          .then([this, retry_policy, backoff_policy](future<void> sleep_fu) {
            sleep_fu.get();  // finish future for sake of finishing
            this->mu_.lock();
            this->impl_ = this->stream_factory_();
            auto start_fu = this->impl_->Start();
            this->mu_.unlock();
            start_fu.then([this](future<bool> start_future) {
              if (!start_future.get()) {
                this->RetryLoop(retry_policy, backoff_policy, retry_type);
              }
              if (retry_type == RetryType.Start) {
                return;
              }
              this->reinitializer_(std::move(this->impl_))
                  .then([this, retry_policy, backoff_policy](
                            future<StatusOr<
                                std::unique_ptr<AsyncStreamingReadWriteRpc<
                                    RequestType, ResponseType>>>>
                                reinitialize_future) {
                    auto reinitialize_response = reinitialize_future.get();
                    if (!reinitialize_response.ok()) {
                      this->RetryLoop(retry_policy, backoff_policy, retry_type);
                    }
                    std::lock_guard(this->retry_loop_mu_);
                    this->mu_.lock();
                    this->impl_ = std::move(*reinitialize_response);
                    this->mu_.unlock();
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
          });
    }
    status_mu_.lock();
    status_promise_.set_value(Status(StatusCode.kAborted, "Permanent error"));
    status_mu_.unlock();
    std::lock_guard(this->retry_loop_mu_);
    this->retry_loop_mu_ = false;
  }

  std::unique_ptr<RetryPolicy const> const retry_policy_prototype_;
  std::unique_ptr<BackoffPolicy const> const backoff_policy_prototype_;
  AsyncSleeper sleeper_;
  StreamFactory<ResponseType, RequestType> const stream_factory_;
  StreamReinitializer<ResponseType, RequestType> const reinitializer_;

  std::mutex mu_;

  std::unique_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>>
      impl_;  // ABSL_GUARDED_BY(mu_)

  std::mutex retry_loop_mu_;

  bool in_retry_loop_;  // ABSL_GUARDED_BY(retry_loop_mu_)
  absl::optional<promise<absl::optional<ResponseType>>>
      read_promise_;  // ABSL_GUARDED_BY(retry_loop_mu_)
  absl::optional<promise<bool>>
      write_promise_;  // ABSL_GUARDED_BY(retry_loop_mu_)

  std::mutex status_mu_;

  promise<Status> status_promise_;  // ABSL_GUARDED_BY(status_mu_)
  future<Status> status_future_;    // ABSL_GUARDED_BY(staus_mu_)
};

/// A helper to apply type deduction.
template <typename ResponseType, typename RequestType>
std::shared_ptr<AsyncStreamingReadWriteRpc<ResponseType, RequestType>>
MakeAsyncResumableStreamingReadWriteRpc(
    std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy, AsyncSleeper sleeper,
    AsyncStreamFactory<ResponseType, RequestType> stream_factory,
    StreamReinitializer<ResponseType, RequestType> reinitializer) {
  return std::make_shared<
      AsyncStreamingReadWriteRpc<ResponseType, RequestType>>(
      std::move(retry_policy), std::move(backoff_policy), sleeper,
      std::move(stream_factory), std::move(reinitializer));
}
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_RESUMABLE_ASYNC_STREAMING_READ_WRITE_RPC_H
