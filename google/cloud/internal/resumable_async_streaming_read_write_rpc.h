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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RESUMABLE_ASYNC_STREAMING_READ_WRITE_RPC_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RESUMABLE_ASYNC_STREAMING_READ_WRITE_RPC_H

#include "google/cloud/async_streaming_read_write_rpc.h"
#include "google/cloud/backoff_policy.h"
#include "google/cloud/internal/async_sleeper.h"
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

using AsyncSleeper = std::function<future<void>(std::chrono::duration)>

    template <typename ResponseType, typename RequestType>
    class ResumableAsyncStreamingReadWriteRpc
    : public AsyncStreamingReadWriteRpc<RequestType, ResponseType> {
 public:
  /**
   * Returns a future that will be completed whenever the stream is completed
   * permanently
   *
   * The consumer of this class should call and wait for Status() when `Read`,
   * `Write`, or `WritesDone` fails.
   */
  virtual future<Status> Status() = 0;
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

  void Cancel() override {
    cancel_mu_.lock();
    is_canceled_ = true;
    cancel_mu_.unlock();
    status_mu_.lock();
    status_promise_.value().set_value(
        Status(StatusCode.kCancelled, "Stream cancelled"));
    status_mu_.unlock();
    std::lock_guard<std::mutex> g{mu_};
    impl_->Cancel();
  }

  future<bool> Start() override {
    status_mu_.lock();
    if (!status_promise_.has_value()) {
      status_promise_ = absl::make_optional(promise<Status>());
      status_future_ = status_promise_.value().get_future();
    }
    status_mu_.unlock();
    mu_.lock();
    auto fu = impl_->Start();
    mu_.unlock();
    return fu.then([this](future<bool> start_fu) {
      if (!start_fu.get()) {
        std::lock_guard(this->retry_loop_mu_);
        if (!this->in_retry_loop_) {
          auto retry_policy = this->retry_policy_prototype_->clone();
          auto backoff_policy = this->backoff_policy_prototype_->clone();
          this->RetryLoop(retry_policy, backoff_policy, RetryType.Write);
          this->in_retry_loop_ = true;
        }
      }
      return make_ready_future(true);
    });
  }

  future<absl::optional<Response>> Read() override {
    status_mu_.lock();
    if (!status_promise_.has_value()) {
      status_promise_ = absl::make_optional(promise<Status>());
      status_future_ = status_promise_.value().get_future();
    }
    status_mu_.unlock();
    mu_.lock();
    auto fu = impl_->Read();
    mu_.unlock();
    return fu.then(
        [this](future<absl::optional<Response>> optional_response_future) {
          this->read_future_ = absl::optional();
          auto optional_response = optional_response_future.get();
          if (optional_response.has_value()) {
            return make_ready_future(optional_response);
          }
          std::lock_guard(this->retry_loop_mu_);
          if (!this->in_retry_loop_) {
            auto retry_policy = this->retry_policy_prototype_->clone();
            auto backoff_policy = this->backoff_policy_prototype_->clone();
            this->RetryLoop(retry_policy, backoff_policy, RetryType.Read);
            this->in_retry_loop_ = true;
          }
          return make_ready_future(absl::optional());
        });
  }

  future<bool> Write(Request const& r, grpc::WriteOptions o) override {
    status_mu_.lock();
    if (!status_promise_.has_value()) {
      status_promise_ = absl::make_optional(promise<Status>());
      status_future_ = status_promise_.value().get_future();
    }
    status_mu_.unlock();
    mu_.lock();
    auto fu = impl_->Write(r, o);
    mu_.unlock();
    return fu.then([this, r](future<bool> write_fu) {
      this->write_future_ = absl::optional();
      if (write_fu.get()) {
        this->write_future_ = absl::optional();
        return make_ready_future(true);
      }
      std::lock_guard(this->retry_loop_mu_);
      if (!this->in_retry_loop_) {
        auto retry_policy = this->retry_policy_prototype_->clone();
        auto backoff_policy = this->backoff_policy_prototype_->clone();
        this->RetryLoop(retry_policy, backoff_policy, RetryType.Write);
        this->in_retry_loop_ = true;
      }
      return make_ready_future(false);
    });
  }

  future<bool> WritesDone() override {
    mu_.lock();
    auto fu = impl_->WritesDone();
    mu_.unlock();

    return fu.then([this](future<bool> writes_done_fu) {
      bool writes_done = writes_done_fu.get();
      this->mu_.lock();
      auto fu = impl_->Finish();
      this->mu_.unlock();
      fu.then([this](future<Status> finish_future) {
        Status finish_status = finish_future.get();
        if (finish_status.ok()) {
          this->finish_status_ = absl::make_optional(finish_status);
          this->status_mu_.lock();
          status_promise_.value().set_value(Status(StatusCode.kOk, "Ok"));
          this->status_mu_.unlock();
          return make_ready_future(true);
        }
        std::lock_guard(this->retry_loop_mu_);
        if (!this->in_retry_loop_) {
          auto retry_policy = this->retry_policy_prototype_->clone();
          auto backoff_policy = this->backoff_policy_prototype_->clone();
          this->RetryLoop(retry_policy, backoff_policy, RetryType.Write);
          this->in_retry_loop_ = true;
        }
        return make_ready_future(false);
      });
    });
  }

  future<Status> Finish() override {
    std::lock_guard<std::mutex> g{mu_};
    if (finish_status_.has_value()) {
      return make_ready_future(finish_status_.value());
    }
    return impl_->Finish();
  }

  future<Status> Status() {
    std::lock_guard(status_mu_);
    return status_future_;
  }

 private:
  enum class RetryType { Start, Read, Write };

  void RetryLoop(std::shared_ptr<RetryPolicy> retry_policy,
                 std::shared_ptr<BackoffPolicy> backoff_policy,
                 RetryType retry_type) {
    cancel_mu_.lock();
    bool is_canceled = is_canceled_;
    cancel_mu_.unlock();
    if (!retry_policy->IsExhausted() && !is_canceled) {
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
                    this->mu_.lock();
                    this->impl_ = std::move(*reinitialize_response);
                    this->mu_.unlock();
                    this->status_mu_.lock();
                    status_promise_.value().set_value(
                        Status(StatusCode.kUnavailable,
                               "Stream failed, May try again"));
                    this->status_mu_.unlock();
                    std::lock_guard(this->retry_loop_mu_);
                    this->retry_loop_mu_ = false;
                  });
            });
          });
    }
    this->status_mu_.lock();
    status_promise_.value().set_value(
        Status(StatusCode.kInternal, "Permanent Error"));
    this->status_mu_.unlock();
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

  absl::optional<Status> finish_status_;

  std::mutex cancel_mu_;

  bool is_canceled_ = false;  // ABSL_GUARDED_BY(cancel_mu_)

  std::mutex status_mu_;

  absl::optional<promise<Status>>
      status_promise_;            // ABSL_GUARDED_BY(status_mu_)
  future<Status> status_future_;  // ABSL_GUARDED_BY(staus_mu_)
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

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RESUMABLE_ASYNC_STREAMING_READ_WRITE_RPC_H
