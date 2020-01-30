// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_POLLING_POLICY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_POLLING_POLICY_H

#include "google/cloud/bigtable/rpc_backoff_policy.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/grpc_utils/grpc_error_delegate.h"
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
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
   * TODO(#2344): remove grpc::Status version.
   */
  virtual bool IsPermanentError(grpc::Status const& status) {
    return IsPermanentError(grpc_utils::MakeStatusFromRpcError(status));
  }

  /**
   * Return true if `status` represents a permanent error that cannot be
   * retried.
   */
  virtual bool IsPermanentError(google::cloud::Status const& status) = 0;

  /**
   * Handle an RPC failure.
   * TODO(#2344): remove grpc::Status version.
   *
   * @return true if the RPC operation should be retried.
   */
  virtual bool OnFailure(grpc::Status const& status) {
    return OnFailure(grpc_utils::MakeStatusFromRpcError(status));
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
 public:
  explicit GenericPollingPolicy(internal::RPCPolicyParameters defaults)
      : rpc_retry_policy_(Retry(defaults)),
        rpc_backoff_policy_(Backoff(defaults)) {}
  GenericPollingPolicy(Retry retry, Backoff backoff)
      : rpc_retry_policy_(std::move(retry)),
        rpc_backoff_policy_(std::move(backoff)) {}

  std::unique_ptr<PollingPolicy> clone() const override {
    return std::unique_ptr<PollingPolicy>(new GenericPollingPolicy(*this));
  }

  void Setup(grpc::ClientContext& context) override {
    rpc_retry_policy_.Setup(context);
    rpc_backoff_policy_.Setup(context);
  }

  bool IsPermanentError(google::cloud::Status const& status) override {
    return RPCRetryPolicy::IsPermanentFailure(status);
  }

  bool OnFailure(google::cloud::Status const& status) override {
    return rpc_retry_policy_.OnFailure(status);
  }

  bool Exhausted() override { return !OnFailure(google::cloud::Status()); }

  std::chrono::milliseconds WaitPeriod() override {
    return rpc_backoff_policy_.OnCompletion(grpc::Status::OK);
  }

 private:
  Retry rpc_retry_policy_;
  Backoff rpc_backoff_policy_;
};

std::unique_ptr<PollingPolicy> DefaultPollingPolicy(
    internal::RPCPolicyParameters defaults);

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_POLLING_POLICY_H
