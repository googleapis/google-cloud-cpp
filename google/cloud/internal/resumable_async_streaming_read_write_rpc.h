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
using AsyncStreamFactory = std::function<std::unique_ptr<
    AsyncStreamingReadWriteRpc<RequestType, ResponseType>>(RequestType)>;

/**
 * `ResumableAsyncStreamingReadWriteRpc<ResponseType, RequestType>` uses
 * callables compatible with this `std::function<>` to update the request object
 * after each response. This is how users of the class can update the resume
 * token or any other parameters needed to restart a stream from the last
 * received message.
 */
template <typename ResponseType, typename RequestType>
using RequestUpdater = std::function<void(ResponseType const&, RequestType&)>;

// Questions
// are locks required because the user can spin up multiple threads to handle Read and Write in parallel?
// where to start for implementing resumable/retry logic for Write since the future only receives a boolean? If it returns true, do we resume?
// Will the request passed to the constructor be for read, and Write will attain its requests just through its arguments?
// How will AsyncStreamFactory work for Read and Write?
// Do I need two different streams, one for Read and one for Write?
// https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/internal/async_read_write_stream_auth.h uses grpc::ClientContext instead?

template <typename ResponseType, typename RequestType>
class ResumableAsyncStreamingReadWriteRpc
    : public AsyncStreamingReadWriteRpc<RequestType, ResponseType> {
 public:
  ResumableAsyncStreamingReadWriteRpc(
      std::unique_ptr<RetryPolicy const> retry_policy,
      std::unique_ptr<BackoffPolicy const> backoff_policy, AsyncSleeper sleeper,
      AsyncStreamFactory<ResponseType, RequestType> stream_factory,
      RequestUpdater<ResponseType, RequestType> updater, RequestType request)
      : retry_policy_prototype_(std::move(retry_policy)),
        backoff_policy_prototype_(std::move(backoff_policy)),
        sleeper_(std::move(sleeper)),
        stream_factory_(std::move(stream_factory)),
        updater_(std::move(updater)),
        request_(std::move(request)),
        impl_(stream_factory_()) {}

  ResumableAsyncStreamingReadWriteRpc(ResumableStreamingReadRpc&&) = delete;
  ResumableAsyncStreamingReadWriteRpc& operator=(ResumableStreamingReadRpc&&) =
      delete;

  void Cancel() override { impl_->Cancel(); }

  future<bool> Start() override {
    return impl_->Start().then([this](future<bool> start_fu) {
      if (!start_fu.get()) {
        return false;
      }
      this->ProcessRead();
    });
  }

  future<StatusOr<Response>> Read() override {
    std::lock_guard<std::mutex> g{mu_};
    if (!read_future_.has_value()) {
      ProcessRead();
    }
    auto fu = read_future_.value();
    read_future_ = absl::optional();
    return fu;
  }

  future<bool> Write(Request const& r, grpc::WriteOptions o) override {
    std::lock_guard<std::mutex> g{mu_};
    return impl_->Write(r, std::move(o));
  }

  future<bool> WritesDone() override {
    std::lock_guard<std::mutex> g{mu_};
    return impl_->WritesDone();
  }

 private:
  void ProcessRead() {
    read_future_ = make_optional(
        impl_->Read().then([this](future<absl::optional<Response>> read_fu) {
          absl::optional<Response> optional_response = read_fu.get();
          if (optional_response.has_value()) {
            return StatusOr(optional_response.value());
          }

          auto const retry_policy = this->retry_policy_prototype_->clone();
          auto const backoff_policy = this->backoff_policy_prototype_->clone();
          while (!retry_policy->IsExhausted()) {
            this->sleeper_(backoff_policy->OnCompletion()).sleep.get();
            this->impl_ = stream_factory_(request_);
            auto fu_response = this->impl_->Read();
            optional_response = fu.get();
            if (optional_response.has_value()) {
              ResponseType response = optional_response.value();
              this->updater_(response, request_);
              return response;
            }
          }
          return StatusOr();
        }));
  }

  std::unique_ptr<RetryPolicy const> const retry_policy_prototype_;
  std::unique_ptr<BackoffPolicy const> const backoff_policy_prototype_;
  AsyncSleeper sleeper_;
  StreamFactory<ResponseType, RequestType> const stream_factory_;
  RequestUpdater<ResponseType, RequestType> const updater_;
  RequestType request_;
  std::unique_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>> impl_;
  absl::optional<future<StatusOr<Response>>> read_future_;
  std::mutex mu_;
};

/// A helper to apply type deduction.
template <typename ResponseType, typename RequestType>
std::shared_ptr<StreamingReadRpc<ResponseType>>
MakeAsyncResumableStreamingReadWriteRpc(
    std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy, AsyncSleeper&& sleeper,
    AsyncStreamFactory<ResponseType, RequestType> stream_factory,
    RequestUpdater<ResponseType, RequestType> updater, RequestType request) {
  return std::make_shared<ResumableStreamingReadRpc<ResponseType, RequestType>>(
      std::move(retry_policy), std::move(backoff_policy),
      std::forward<AsyncSleeper>(sleeper), std::move(stream_factory),
      std::move(updater), std::forward<RequestType>(request));
}
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RESUMABLE_ASYNC_STREAMING_READ_WRITE_RPC_H
