// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_POLLING_POLICY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_POLLING_POLICY_H

#include "google/cloud/bigtable/rpc_backoff_policy.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/grpc_error_delegate.h"
#include <grpcpp/grpcpp.h>
#include <chrono>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
/**
 * Define the interface for providing asynchronous repetitive call rules
 *
 */
class PollingPolicy {
 public:
  virtual ~PollingPolicy() = default;

  /**
   * Return a new copy of this object.
   *
   * Typically implemented as
   * @code
   *   return std::unique_ptr<PollingPolicy>(new Foo(*this));
   * @endcode
   */
  virtual std::unique_ptr<PollingPolicy> clone() const = 0;

  virtual void Setup(grpc::ClientContext& context) = 0;

  /**
   * Return true if `status` represents a permanent error that cannot be
   * retried.
   * TODO(#2344): remove `grpc::Status` version.
   */
  virtual bool IsPermanentError(grpc::Status const& status) {
    return IsPermanentError(MakeStatusFromRpcError(status));
  }

  /**
   * Return true if `status` represents a permanent error that cannot be
   * retried.
   */
  virtual bool IsPermanentError(google::cloud::Status const& status) = 0;

  /**
   * Handle an RPC failure.
   * TODO(#2344): remove `grpc::Status` version.
   *
   * @return true if the RPC operation should be retried.
   */
  virtual bool OnFailure(grpc::Status const& status) {
    return OnFailure(MakeStatusFromRpcError(status));
  }

  /**
   * Handle an RPC failure.
   *
   * @return true if the RPC operation should be retried.
   */
  virtual bool OnFailure(google::cloud::Status const& status) = 0;

  /**
   * Return true if we cannot try again.
   */
  virtual bool Exhausted() = 0;

  /**
   * Return for how long we should wait before trying again.
   */
  virtual std::chrono::milliseconds WaitPeriod() = 0;
};

/**
 * Construct a polling policy from existing Retry and Backoff policies.
 *
 * A polling policy can be built by composing a retry and backoff policy. For
 * example, to create a polling policy that "retries N times, waiting a fixed
 * period between retries" you could compose the "try N times" retry policy with
 * the "wait a fixed period between retries".
 *
 * This class makes it easier to create such composed polling policies.
 *
 * @tparam Retry the RPC retry strategy used to limit the number or the total
 *     duration of the polling strategy.
 * @tparam Backoff the RPC backoff strategy used to control how often the
 *     library polls.
 */
template <typename Retry = LimitedTimeRetryPolicy,
          typename Backoff = ExponentialBackoffPolicy>
class GenericPollingPolicy : public PollingPolicy {
  static_assert(std::is_convertible<Retry*, RPCRetryPolicy*>::value,
                "Retry must derive from RPCRetryPolicy");
  static_assert(std::is_convertible<Backoff*, RPCBackoffPolicy*>::value,
                "Backoff must derive from RPCBackoffPolicy");

 public:
  explicit GenericPollingPolicy(internal::RPCPolicyParameters defaults)
      : rpc_retry_policy_(Retry(defaults)),
        rpc_backoff_policy_(Backoff(defaults)),
        retry_clone_(rpc_retry_policy_.clone()),
        backoff_clone_(rpc_backoff_policy_.clone()) {}
  GenericPollingPolicy(Retry retry, Backoff backoff)
      : rpc_retry_policy_(std::move(retry)),
        rpc_backoff_policy_(std::move(backoff)),
        retry_clone_(rpc_retry_policy_.clone()),
        backoff_clone_(rpc_backoff_policy_.clone()) {}

  std::unique_ptr<PollingPolicy> clone() const override {
    return std::unique_ptr<PollingPolicy>(
        new GenericPollingPolicy<Retry, Backoff>(rpc_retry_policy_,
                                                 rpc_backoff_policy_));
  }

  void Setup(grpc::ClientContext& context) override {
    retry_clone_->Setup(context);
    backoff_clone_->Setup(context);
  }

  bool IsPermanentError(google::cloud::Status const& status) override {
    return RPCRetryPolicy::IsPermanentFailure(status);
  }

  bool OnFailure(google::cloud::Status const& status) override {
    return retry_clone_->OnFailure(status);
  }

  bool Exhausted() override { return !OnFailure(google::cloud::Status()); }

  std::chrono::milliseconds WaitPeriod() override {
    return backoff_clone_->OnCompletion(grpc::Status::OK);
  }

 private:
  Retry rpc_retry_policy_;
  Backoff rpc_backoff_policy_;

  std::unique_ptr<RPCRetryPolicy> retry_clone_;
  std::unique_ptr<RPCBackoffPolicy> backoff_clone_;
};

std::unique_ptr<PollingPolicy> DefaultPollingPolicy(
    internal::RPCPolicyParameters defaults);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_POLLING_POLICY_H
