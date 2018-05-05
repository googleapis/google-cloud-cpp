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

#include <grpc++/grpc++.h>

#include "bigtable/client/rpc_backoff_policy.h"
#include "bigtable/client/rpc_retry_policy.h"
#include "bigtable/client/version.h"

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
  virtual bool IsPermanentError(grpc::StatusCode const& status_code) = 0;

  /**
   * Return true if we cannot try again.
   */
  virtual bool Exhausted() = 0;

  /**
   * Return for how long we should wait before trying again.
   */
  virtual std::chrono::milliseconds WaitPeriod() = 0;
};

// Define the defaults using a pre-processor macro, this allows the application
// developers to change the defaults for their application by compiling with
// different values.
#ifndef BIGTABLE_CLIENT_DEFAULT_MAXIMUM_RETRY_PERIOD
#define BIGTABLE_CLIENT_DEFAULT_MAXIMUM_RETRY_PERIOD std::chrono::minutes(6)
#endif  // BIGTABLE_CLIENT_DEFAULT_MAXIMUM_RETRY_PERIOD

const auto maximum_retry_period = BIGTABLE_CLIENT_DEFAULT_MAXIMUM_RETRY_PERIOD;

#ifndef BIGTABLE_CLIENT_DEFAULT_INITIAL_DELAY
#define BIGTABLE_CLIENT_DEFAULT_INITIAL_DELAY std::chrono::milliseconds(10)
#endif  // BIGTABLE_CLIENT_DEFAULT_INITIAL_DELAY

#ifndef BIGTABLE_CLIENT_DEFAULT_MAXIMUM_DELAY
#define BIGTABLE_CLIENT_DEFAULT_MAXIMUM_DELAY std::chrono::minutes(5)
#endif  // BIGTABLE_CLIENT_DEFAULT_MAXIMUM_DELAY

const auto default_initial_delay = BIGTABLE_CLIENT_DEFAULT_INITIAL_DELAY;
const auto default_maximum_delay = BIGTABLE_CLIENT_DEFAULT_MAXIMUM_DELAY;

template <typename Retry = LimitedTimeRetryPolicy,
          typename Backoff = ExponentialBackoffPolicy>
class GenericPollingPolicy : public PollingPolicy,
                             private Retry,
                             private Backoff {
 public:
  GenericPollingPolicy()
      : Retry(maximum_retry_period),
        Backoff(default_initial_delay, default_maximum_delay) {}
  GenericPollingPolicy(Retry retry, Backoff backoff)
      : Retry(std::move(retry)), Backoff(std::move(backoff)) {}

  std::unique_ptr<PollingPolicy> clone() override {
    return std::unique_ptr<PollingPolicy>(new GenericPollingPolicy(*this));
  }

  bool IsPermanentError(grpc::StatusCode const& status_code) override {
    return not Retry::can_retry(status_code);
  }

  bool Exhausted() override { return not Retry::on_failure(grpc::Status::OK); }

  std::chrono::milliseconds WaitPeriod() override {
    return Backoff::on_completion(grpc::Status::OK);
  }
};

std::unique_ptr<PollingPolicy> DefaultPollingPolicy();

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_POLLING_POLICY_H_
