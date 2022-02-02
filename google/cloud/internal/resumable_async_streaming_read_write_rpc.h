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
using StreamReinitializer = std::function<future<Status>(
    std::unique_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>>&,
    RequestType&)>;

// TODOs
// DRY retry loop
// update Write/WritesDone status through Status() function

template <typename ResponseType, typename RequestType>
class ResumableAsyncStreamingReadWriteRpc
    : public AsyncStreamingReadWriteRpc<RequestType, ResponseType> {
 public:
  ResumableAsyncStreamingReadWriteRpc(
      std::unique_ptr<RetryPolicy const> retry_policy,
      std::unique_ptr<BackoffPolicy const> backoff_policy,
      const AsyncSleeper& sleeper,
      AsyncStreamFactory<ResponseType, RequestType> stream_factory,
      StreamReinitializer<ResponseType, RequestType> reinitializer)
      : retry_policy_prototype_(std::move(retry_policy)),
        backoff_policy_prototype_(std::move(backoff_policy)),
        sleeper_(std::move(sleeper)),
        stream_factory_(std::move(stream_factory)),
        reinitializer_(std::move(reinitializer)),
        impl_(stream_factory_()) {}

  ResumableAsyncStreamingReadWriteRpc(ResumableStreamingReadRpc&&) = delete;
  ResumableAsyncStreamingReadWriteRpc& operator=(ResumableStreamingReadRpc&&) =
      delete;

  void Cancel() override {
    std::lock_guard<std::mutex> g{mu_};
    impl_->Cancel();
  }

  future<bool> Start() override {
    return impl_->Start().then([this](future<bool> start_fu) {
      if (!start_fu.get()) {
        return false;
      }
      std::lock_guard<std::mutex> g{this->mu_};
      this->impl_ = this->stream_factory_();
      return true;
    });
  }

  future<StatusOr<Response>> Read(Request const& r) override {
    std::lock_guard<std::mutex> g{mu_};
    if (!read_future_.has_value()) {
      ProcessRead(r);
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
      Status finish_status = impl_->Finish().get();
      if (finish_status.ok()) {
        this->finish_status_ = absl::make_optional(finish_status);
        return writes_done;
      }
      auto const retry_policy = this->retry_policy_prototype_->clone();
      auto const backoff_policy = this->backoff_policy_prototype_->clone();
      while (!retry_policy->IsExhausted()) {
        this->sleeper_(backoff_policy->OnCompletion()).sleep.get();
        this->impl_ = this->stream_factory_();
        bool start = this->impl_->Start().get();
        if (!start) {
          continue;
        }
      }
      return false;
    });
  }

  future<Status> Finish() override {
    std::lock_guard<std::mutex> g{mu_};
    if (finish_status_.has_value()) {
      return make_ready_future(finish_status_.value());
    }
    return impl_->Finish();
  }

  virtual future<Status> Status() = 0;

 private:
  void ProcessRead(Request const& r) {
    read_future_ = make_optional(
        impl_->Read().then([this, r](future<absl::optional<Response>> read_fu) {
          this->read_future_ = absl::optional();
          absl::optional<Response> optional_response = read_fu.get();
          if (optional_response.has_value()) {
            return StatusOr(optional_response.value());
          }
          auto const retry_policy = this->retry_policy_prototype_->clone();
          auto const backoff_policy = this->backoff_policy_prototype_->clone();
          std::lock_guard<std::mutex> g{mu_};
          while (!retry_policy->IsExhausted()) {
            this->sleeper_(backoff_policy->OnCompletion()).sleep.get();
            this->impl_ = this->stream_factory_();
            bool start = this->impl_->Start().get();
            if (!start) {
              continue;
            }
            Status reinitialize_status =
                this->reinitializer_(this->impl_, r).get();
            if (!reinitialize_status.ok()) {
              continue;
            }
            return StatusOr(
                Status(StatusCode.kDataLoss, "Stream failed, May try again"));
          }
          return StatusOr(Status(StatusCode.kUnavailable,
                                 "Tried to reinitialize stream. Failed."));
        }));
  }

  void ProcessWrite(Request const& r, grpc::WriteOptions o) {
    write_future_ =
        make_optional(impl_->Write(r, o).then([this, r](future<bool> write_fu) {
          this->write_future_ = absl::optional();
          bool response = write_fu.get();
          if (response) {
            this->write_future_ = absl::optional();
            return true;
          }

          auto const retry_policy = this->retry_policy_prototype_->clone();
          auto const backoff_policy = this->backoff_policy_prototype_->clone();
          std::lock_guard<std::mutex> g{mu_};
          while (!retry_policy->IsExhausted()) {
            this->sleeper_(backoff_policy->OnCompletion()).sleep.get();
            this->impl_ = this->stream_factory_();
            bool start = this->impl_->Start().get();
            if (!start) {
              continue;
            }
            Status reinitialize_status =
                this->reinitializer_(this->impl_, r).get();
            if (!reinitialize_status.ok()) {
              continue;
            }
          }
          return false;
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
  absl::optional<future<StatusOr<Response>>> read_future_;
  absl::optional<future<bool>> write_future_;
};

/// A helper to apply type deduction.
template <typename ResponseType, typename RequestType>
std::shared_ptr<AsyncStreamingReadWriteRpc<ResponseType, RequestType>>
MakeAsyncResumableStreamingReadWriteRpc(
    std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy, AsyncSleeper&& sleeper,
    AsyncStreamFactory<ResponseType, RequestType> stream_factory,
    StreamReinitializer<ResponseType, RequestType> reinitializer) {
  return std::make_shared<
      AsyncStreamingReadWriteRpc<ResponseType, RequestType>>(
      std::move(retry_policy), std::move(backoff_policy),
      std::forward<AsyncSleeper>(sleeper), std::move(stream_factory),
      std::move(reinitializer));
}
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RESUMABLE_ASYNC_STREAMING_READ_WRITE_RPC_H
