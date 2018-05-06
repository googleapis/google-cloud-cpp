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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_POLLING_POLICY_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_POLLING_POLICY_H_

#include "bigtable/client/rpc_backoff_policy.h"
#include "bigtable/client/rpc_retry_policy.h"
#include "bigtable/client/version.h"
#include <grpc++/grpc++.h>

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
  virtual std::unique_ptr<PollingPolicy> clone() = 0;

  /**
   * Return true if `status` represents a permanent error that cannot be
   * retried.
   */
  virtual bool IsPermanentError(grpc::Status const& status) = 0;

  /**
   * Handle an RPC failure.
   *
   * @return true if the RPC operation should be retried.
   */
  virtual bool OnFailure(grpc::Status const& status) = 0;

  /**
   * Return true if we cannot try again.
   */
  virtual bool Exhausted() = 0;

  /**
   * Return for how long we should wait before trying again.
   */
  virtual std::chrono::milliseconds WaitPeriod() = 0;
};

template <typename Retry = LimitedTimeRetryPolicy,
          typename Backoff = ExponentialBackoffPolicy>
class GenericPollingPolicy : public PollingPolicy,
                             private Retry,
                             private Backoff {
 public:
  GenericPollingPolicy() : Retry(), Backoff() {}
  GenericPollingPolicy(Retry retry, Backoff backoff)
      : Retry(std::move(retry)), Backoff(std::move(backoff)) {}

  using PollingPolicy::clone;
  std::unique_ptr<PollingPolicy> clone() {
    return std::unique_ptr<PollingPolicy>(new GenericPollingPolicy(*this));
  }

  bool IsPermanentError(grpc::Status const& status) override {
    return not Retry::can_retry(status.error_code());
  }

  bool OnFailure(grpc::Status const& status) override {
    return Retry::on_failure(status);
  }

  bool Exhausted() override { return not OnFailure(grpc::Status::OK); }

  std::chrono::milliseconds WaitPeriod() override {
    return Backoff::on_completion(grpc::Status::OK);
  }
};

std::unique_ptr<PollingPolicy> DefaultPollingPolicy();

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_POLLING_POLICY_H_
