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
#include "google/cloud/version.h"
#include "google/cloud/status_or.h"
#include <chrono>
#include <memory>
#include <thread>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * `ResumableAsyncStreamingReadWriteRpc<ResponseType, RequestType>` uses callables
 * compatible with this `std::function<>` to create new streams.
 */
template <typename ResponseType, typename RequestType>
using AsyncStreamFactory =
    std::function<std::unique_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>>()>;

/**
 * `ResumableAsyncStreamingReadWriteRpc<ResponseType, RequestType>` uses callables
 * compatible with this `std::function<>` to update the request object after
 * each response. This is how users of the class can update the resume token or
 * any other parameters needed to restart a stream from the last received
 * message.
 */
template <typename ResponseType, typename RequestType>
using RequestUpdater = std::function<void(ResponseType const&, RequestType&)>;

/**
 * A StreamingReadRpc that resumes on transient failures.
 *
 * This class implements the "resume loop", an analog to `RetryLoop()` for
 * streaming read RPCs.
 *
 * Often streaming read RPCs are used to implement "downloads", or large reads
 * over potentially unbounded amounts of data. Many services provide a mechanism
 * to "resume" these streaming RPCs if the operation is interrupted in the
 * middle. That is, the service may be able to restart the streaming RPC from
 * the item following the last received entry. This is useful because one may
 * not want to perform one half of a large download (think TiBs of data) more
 * than once.
 *
 * When the service provides such a "resume" mechanism it is typically
 * implemented as string or byte token returned in each response. Sending the
 * last received token in the "resume" request signals that the operation should
 * skip the data received before the token.
 *
 * When implementing the resume loop it is important to reset any retry policies
 * after any progress is made. The retry policy is interpreted as the limit on
 * the time or number of attempts to *start* a streaming RPC, not a limit on the
 * total time for the streaming RPC.
 */

// TODO make latter 3 into interfaces
// what will latter two interfaces contain?
template <typename ResponseType, typename RequestType, typename RetryPolicy,
          typename BackoffPolicy, typename Sleeper>
class ResumableAsyncStreamingReadWriteRpc : public AsyncStreamingReadWriteRpc<RequestType, ResponseType> {
 public:
  ResumableAsyncStreamingReadWriteRpc(
      std::unique_ptr<RetryPolicy const> retry_policy,
      std::unique_ptr<BackoffPolicy const> backoff_policy, Sleeper sleeper,
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
  ResumableAsyncStreamingReadWriteRpc& operator=(ResumableStreamingReadRpc&&) = delete;

  void Cancel() override { impl_->Cancel(); }

  future<bool> Start() override {
    // TODO
    return impl_->Start();
  }

  future<StatusOr<Response>> Read() override {
    // currently working on
    if (pending_read_) {
      promise<StatusOr<Response>> p;
      // use different status code
      p.set_value(Status(StatusCode.kAlreadyExists, "Pending read operation already present."));
      return p.get_future();
    }
    pending_read_ = true;
    return std::async(ProcessRead, std::move(impl_->read()));
  }

  future<bool> Write() override {
    // TODO
    return future<bool>();
  }

 private:
  StatusOr<Response> ProcessRead(future<absl::optional<Response>> fu) {
    absl::optional<Response> optional_response = fu.get();
    if (optional_response.has_value()) {
      pending_read_ = false;
      return StatusOr(optional_response.value());
    }

    auto const retry_policy = retry_policy_prototype_->clone();
    auto const backoff_policy = backoff_policy_prototype_->clone();
    // why does resumable_streaming_read_rpc.h use has_received_data here?
    while (!retry_policy->IsExhausted()) {
      sleeper_(backoff_policy->OnCompletion());
      has_received_data_ = false;
      // lock this, so calls to Write aren't corrupted?
      impl_ = stream_factory_(request_);
      auto fu_response = impl_->Read();
      optional_response = fu.get();
      if (optional_response.has_value()) {
        ResponseType response = optional_response.value();
        updater_(response, request_);
        has_received_data_ = true;
        pending_read_ = false;
        return response;
      }
    }
    // how to get more specific status from absl::optional?
    pending_read_ = false;
    return StatusOr();
  }

//  StreamingRpcMetadata GetRequestMetadata() const override {
//    return impl_ ? impl_->GetRequestMetadata() : StreamingRpcMetadata{};
//  }

 private:
  std::unique_ptr<RetryPolicy const> const retry_policy_prototype_;
  std::unique_ptr<BackoffPolicy const> const backoff_policy_prototype_;
  Sleeper sleeper_;
  StreamFactory<ResponseType, RequestType> const stream_factory_;
  RequestUpdater<ResponseType, RequestType> const updater_;
  RequestType request_;
  std::unique_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>> impl_;
  bool has_received_data_ = false;
  bool pending_read_ = false;
  bool pending_write_ = false;
};

// TODO refactor for current class
/// A helper to apply type deduction.
template <typename ResponseType, typename RequestType, typename RetryPolicy,
          typename BackoffPolicy, typename Sleeper>
std::shared_ptr<StreamingReadRpc<ResponseType>> MakeResumableStreamingReadRpc(
    std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy, Sleeper&& sleeper,
    StreamFactory<ResponseType, RequestType> stream_factory,
    RequestUpdater<ResponseType, RequestType> updater, RequestType request) {
  return std::make_shared<
      ResumableStreamingReadRpc<ResponseType, RequestType, RetryPolicy,
                                BackoffPolicy, absl::decay_t<Sleeper>>>(
      std::move(retry_policy), std::move(backoff_policy),
      std::forward<Sleeper>(sleeper), std::move(stream_factory),
      std::move(updater), std::forward<RequestType>(request));
}

/// A helper to apply type deduction, with the default sleeping strategy.
template <typename ResponseType, typename RequestType, typename RetryPolicy,
          typename BackoffPolicy>
std::shared_ptr<StreamingReadRpc<ResponseType>> MakeResumableStreamingReadRpc(
    std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy,
    StreamFactory<ResponseType, RequestType> stream_factory,
    RequestUpdater<ResponseType, RequestType> updater, RequestType request) {
  return MakeResumableStreamingReadRpc(
      std::move(retry_policy), std::move(backoff_policy),
      [](std::chrono::milliseconds d) { std::this_thread::sleep_for(d); },
      std::move(stream_factory), std::move(updater), std::move(request));
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RESUMABLE_ASYNC_STREAMING_READ_WRITE_RPC_H
