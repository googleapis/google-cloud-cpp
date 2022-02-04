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
 * callables compatible with this `std::function<>` to update the request object
 * after each response. This is how users of the class can update the resume
 * token or any other parameters needed to restart a stream from the last
 * received message.
 */
template <typename ResponseType, typename RequestType>
using StreamReinitializer = std::function<future<StatusOr<
    std::unique_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>>>>(
    std::unique_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>>)>;

template <typename ResponseType, typename RequestType>
class ResumableAsyncStreamingReadWriteRpc
    : public AsyncStreamingReadWriteRpc<RequestType, ResponseType> {
 public:
  virtual future<Status> Status() = 0;
};

template <typename ResponseType, typename RequestType>
class ResumableAsyncStreamingReadWriteRpcImpl
    : public ResumableAsyncStreamingReadWriteRpc<RequestType, ResponseType> {
 public:
  ResumableAsyncStreamingReadWriteRpcImpl(
      std::unique_ptr<RetryPolicy const> retry_policy,
      std::unique_ptr<BackoffPolicy const> backoff_policy,
      const AsyncSleeper& sleeper,
      AsyncStreamFactory<ResponseType, RequestType>& stream_factory,
      StreamReinitializer<ResponseType, RequestType>& reinitializer)
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
    // not guarding since this is the singular writer
    is_canceled_ = true;
    std::lock_guard<std::mutex> g{mu_};
    impl_->Cancel();
  }

  future<bool> Start() override {
    std::lock_guard<std::mutex> g{mu_};
    return impl_->Start().then([this](future<bool> start_fu) {
      if (!start_fu.get()) {
        std::shared_ptr<RetryPolicy const> retry_policy =
            this->retry_policy_prototype_->clone();
        std::shared_ptr<BackoffPolicy const> backoff_policy backoff_policy =
            this->backoff_policy_prototype_->clone();
        return this->RetryLoop(retry_policy, backoff_policy, RetryType.Start);
      }
      return make_ready_future(true);
    });
  }

  future<absl::optional<Response>> Read() override {
    std::lock_guard<std::mutex> g{mu_};
    if (!read_future_.has_value()) {
      ProcessRead();
    }
    return read_future_.value();
  }

  future<bool> Write(Request const& r, grpc::WriteOptions o) override {
    std::lock_guard<std::mutex> g{mu_};
    if (!write_future_.has_value()) {
      ProcessWrite(r, o);
    }
    return write_future_.value();
  }

  future<bool> WritesDone() override {
    std::lock_guard<std::mutex> g{mu_};
    return impl_->WritesDone().then([this](future<bool> writes_done_fu) {
      bool writes_done = writes_done_fu.get();
      std::lock_guard<std::mutex> g{this->mu_};
      impl_->Finish().then([this](future<Status> finish_future) {
        Status finish_status = finish_future.get();
        if (finish_status.ok()) {
          this->finish_status_ = absl::make_optional(finish_status);
          return make_ready_future(true);
        }
        auto const retry_policy = this->retry_policy_prototype_->clone();
        auto const backoff_policy = this->backoff_policy_prototype_->clone();
        return this->RetryLoop(retry_policy, backoff_policy, RetryType.Write);
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
    std::lock_guard<std::mutex> g{mu_};
    return make_ready_future(current_status_);
  }

 private:
  enum class RetryType { Status, Read, Write };

  absl::variant<future<absl::optional<ResponseType>>, future<bool>> RetryLoop(
      std::shared_ptr<RetryPolicy const> retry_policy,
      std::shared_ptr<BackoffPolicy const> backoff_policy,
      RetryType retry_type) {
    std::lock_guard<std::mutex> g{mu_};
    if (!retry_policy->IsExhausted() && !is_canceled) {
      return this->sleeper_(backoff_policy->OnCompletion())
          .sleep()
          .then([this, retry_policy, backoff_policy](future<void>) {
            std::lock_guard<std::mutex> g{this->mu_};
            this->impl_ = this->stream_factory_();
            return this->impl_->Start().then([this](future<bool> start_future) {
              if (!start_future.get()) {
                return this->RetryLoop(retry_policy, backoff_policy,
                                       retry_type);
              }
              if (retry_type == RetryType.Start) {
                return make_ready_future(true);
              }
              return this->reinitializer_(std::move(this->impl_))
                  .then([this, retry_policy, backoff_policy](
                            future<StatusOr<
                                std::unique_ptr<AsyncStreamingReadWriteRpc<
                                    RequestType, ResponseType>>>>
                                reinitialize_future) {
                    StatusOr<std::unique_ptr<
                        AsyncStreamingReadWriteRpc<RequestType, ResponseType>>>
                        reinitialize_response = reinitialize_future.get();
                    if (!reinitialize_response.ok()) {
                      return this->RetryLoop(retry_policy, backoff_policy,
                                             retry_type);
                    }
                    std::lock_guard<std::mutex> g{this->mu_};
                    this->impl_ = std::move(*reinitialize_response);
                    this->current_status_ = Status(
                        StatusCode.kDataLoss, "Stream failed, May try again");
                    if (retry_type == RetryType.Read) {
                      return make_ready_future(absl::optional());
                    }
                    if (retry_type == RetryType.Status) {
                      return make_ready_future(true);
                    }
                    return make_ready_future(false);
                  });
            });
          });
    }
    current_status_ = Status(StatusCode.kUnavailable, "Permanent Error");
    if (retry_type == RetryType.Read) {
      return make_ready_future(absl::optional());
    }
    if (retry_type == RetryType.Status) {
      return make_ready_future(true);
    }
    return make_ready_future(false);
  }

  void ProcessRead() {
    read_future_ = make_optional(impl_->Read().then(
        [this](future<absl::optional<Response>> optional_response_future) {
          this->read_future_ = absl::optional();
          absl::optional<Response> optional_response =
              optional_response_future.get();
          if (optional_response.has_value()) {
            return make_ready_future(optional_response);
          }
          std::shared_ptr<RetryPolicy const> retry_policy =
              this->retry_policy_prototype_->clone();
          std::shared_ptr<BackoffPolicy const> backoff_policy backoff_policy =
              this->backoff_policy_prototype_->clone();
          return this->RetryLoop(retry_policy, backoff_policy, RetryType.Read);
        }));
  }

  void ProcessWrite(Request const& r, grpc::WriteOptions o) {
    write_future_ =
        make_optional(impl_->Write(r, o).then([this, r](future<bool> write_fu) {
          this->write_future_ = absl::optional();
          if (write_fu.get()) {
            this->write_future_ = absl::optional();
            return make_ready_future(true);
          }
          std::shared_ptr<RetryPolicy const> retry_policy =
              this->retry_policy_prototype_->clone();
          auto const backoff_policy = this->backoff_policy_prototype_->clone();
          return this->RetryLoop(retry_policy, backoff_policy, RetryType.Write);
        }));
  }

  std::unique_ptr<RetryPolicy const> const retry_policy_prototype_;
  std::unique_ptr<BackoffPolicy const> const backoff_policy_prototype_;
  AsyncSleeper sleeper_;
  StreamFactory<ResponseType, RequestType> const stream_factory_;
  StreamReinitializer<ResponseType, RequestType> const reinitializer_;
  std::unique_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>> impl_;
  std::mutex mu_;
  absl::optional<Status> finish_status_;
  Status current_status_;
  bool is_canceled_ = false;
  absl::optional<future<StatusOr<Response>>> read_future_;
  absl::optional<future<bool>> write_future_;
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
