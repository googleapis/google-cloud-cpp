// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RESUMABLE_STREAMING_READ_RPC_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RESUMABLE_STREAMING_READ_RPC_H

#include "google/cloud/internal/streaming_read_rpc.h"
#include "google/cloud/version.h"
#include <memory>
#include <thread>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

template <typename ResponseType, typename RequestType>
using StreamFactory =
    std::function<std::unique_ptr<StreamingReadRpc<ResponseType>>(RequestType)>;

template <typename ResponseType, typename RequestType>
using RequestUpdater = std::function<void(RequestType&, ResponseType const&)>;

/**
 * A StreamingReadRpc that resumes on transient failures.
 *
 *
 */
template <typename ResponseType, typename RequestType, typename RetryPolicy,
          typename BackoffPolicy, typename Sleeper>
class ResumableStreamingReadRpc : public StreamingReadRpc<ResponseType> {
 public:
  ResumableStreamingReadRpc(
      std::unique_ptr<RetryPolicy const> retry_policy,
      std::unique_ptr<BackoffPolicy const> backoff_policy, Sleeper sleeper,
      StreamFactory<ResponseType, RequestType> stream_factory,
      RequestUpdater<ResponseType, RequestType> updater, RequestType request)
      : retry_policy_prototype_(std::move(retry_policy)),
        backoff_policy_prototype_(std::move(backoff_policy)),
        sleeper_(std::move(sleeper)),
        stream_factory_(std::move(stream_factory)),
        updater_(std::move(updater)),
        request_(std::move(request)),
        impl_(stream_factory_(request_)) {}

  void Cancel() override { impl_->Cancel(); }

  absl::variant<Status, ResponseType> Read() override {
    auto response = impl_->Read();
    if (absl::holds_alternative<ResponseType>(response)) {
      updater_(request_, absl::get<ResponseType>(response));
      return response;
    }
    auto last_status = absl::get<Status>(std::move(response));
    if (last_status.ok()) return last_status;
    // Need to start a retry loop to connect again. Note that we *retry* to
    // start a streaming read, but once the streaming read succeeds at least
    // once we *resume* the read using *fresh* retry and backoff policies.
    //
    // This is important because streaming reads can last very long, many
    // minutes or hours, maybe much longer than the retry policy. For example,
    // consider a retry policy of "try for 5 minutes" and a streaming read that
    // works for 1 hours and then gets interrupted, in this case it would be
    // better to resume the read, giving up after 5 minutes of retries, than
    // just aborting because the retry policy is from one hour ago.
    auto const retry_policy = retry_policy_prototype_->clone();
    auto const backoff_policy = backoff_policy_prototype_->clone();
    while (!retry_policy->IsExhausted()) {
      impl_ = stream_factory_(request_);
      auto r = impl_->Read();
      if (absl::holds_alternative<ResponseType>(r)) {
        updater_(request_, absl::get<ResponseType>(r));
        return r;
      }
      last_status = absl::get<Status>(std::move(r));
      if (!retry_policy->OnFailure(last_status)) return last_status;
      sleeper_(backoff_policy->OnCompletion());
    }
    return last_status;
  }

 private:
  std::unique_ptr<RetryPolicy const> const retry_policy_prototype_;
  std::unique_ptr<BackoffPolicy const> const backoff_policy_prototype_;
  Sleeper sleeper_;
  StreamFactory<ResponseType, RequestType> const stream_factory_;
  RequestUpdater<ResponseType, RequestType> const updater_;
  RequestType request_;
  std::unique_ptr<StreamingReadRpc<ResponseType>> impl_;
};

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
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RESUMABLE_STREAMING_READ_RPC_H
