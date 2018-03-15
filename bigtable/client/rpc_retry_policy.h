// Copyright 2017 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_RPC_RETRY_POLICY_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_RPC_RETRY_POLICY_H_

#include "bigtable/client/version.h"
#include <grpc++/grpc++.h>
#include <chrono>
#include <memory>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Define the interface for controlling how the Bigtable client
 * retries RPC operations.
 *
 * The C++ client for Bigtable needs to hide partial and temporary
 * failures from the application.  However, we need to give the users
 * enough flexibility to control how many attempts are made to reissue
 * operations, how often these attempts are executed, and how to
 * signal that an error has occurred.
 *
 * The application provides an instance of this class when the Table
 * (or TableAdmin) object is created.  This instance serves as a
 * prototype to create new RPCRetryPolicy objects of the same
 * (dynamic) type and with the same initial state.
 *
 */
class RPCRetryPolicy {
 public:
  virtual ~RPCRetryPolicy() = default;

  /**
   * Return a new copy of this object.
   *
   * Typically implemented as
   * @code
   *   return std::unique_ptr<RPCRetryPolicy>(new Foo(*this));
   * @endcode
   */
  virtual std::unique_ptr<RPCRetryPolicy> clone() const = 0;

  /**
   * Update the ClientContext for the next call.
   */
  virtual void setup(grpc::ClientContext& context) const = 0;

  /**
   * Handle an RPC failure.
   *
   * @return true if the RPC operation should be retried.
   */
  virtual bool on_failure(grpc::Status const& status) = 0;

  /// Return true if the status code is retryable.
  virtual bool can_retry(grpc::StatusCode code) const = 0;
};

/// Return an instance of the default RPCRetryPolicy.
std::unique_ptr<RPCRetryPolicy> DefaultRPCRetryPolicy();

/**
 * Implement a simple "count errors and then stop" retry policy.
 */
class LimitedErrorCountRetryPolicy : public RPCRetryPolicy {
 public:
  explicit LimitedErrorCountRetryPolicy(int maximum_failures)
      : failure_count_(0), maximum_failures_(maximum_failures) {}

  std::unique_ptr<RPCRetryPolicy> clone() const override;
  void setup(grpc::ClientContext& context) const override;
  bool on_failure(grpc::Status const& status) override;
  bool can_retry(grpc::StatusCode code) const override;

 private:
  int failure_count_;
  int maximum_failures_;
};

/**
 * Implement a simple "keep trying for this time" retry policy.
 */
class LimitedTimeRetryPolicy : public RPCRetryPolicy {
 public:
  template <typename duration_t>
  explicit LimitedTimeRetryPolicy(duration_t maximum_duration)
      : maximum_duration_(std::chrono::duration_cast<std::chrono::milliseconds>(
            maximum_duration)),
        deadline_(std::chrono::system_clock::now() + maximum_duration_) {}

  std::unique_ptr<RPCRetryPolicy> clone() const override;
  void setup(grpc::ClientContext& context) const override;
  bool on_failure(grpc::Status const& status) override;
  bool can_retry(grpc::StatusCode code) const override;

 private:
  std::chrono::milliseconds maximum_duration_;
  std::chrono::system_clock::time_point deadline_;
};

/// The most common retryable codes, refactored because it is used in several
/// places.
constexpr bool IsRetryableStatusCode(grpc::StatusCode code) {
  return code == grpc::StatusCode::OK or code == grpc::StatusCode::ABORTED or
         code == grpc::StatusCode::UNAVAILABLE or
         code == grpc::StatusCode::DEADLINE_EXCEEDED;
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_RPC_RETRY_POLICY_H_
