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

#ifndef BIGTABLE_RPC_CLIENT_RETRY_POLICY_H_
#define BIGTABLE_RPC_CLIENT_RETRY_POLICY_H_

#include <bigtable/client/version.h>

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
  //
  virtual ~RPCRetryPolicy() = 0;

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
   * Handle an RPC failure.
   *
   * @return true if the RPC operation should be retried.
   * @param delay how long to wait before retrying the operation.
   */
  virtual bool on_failure(grpc::Status const& status,
                          std::chrono::milliseconds* delay) = 0;
};

/// Return an instance of the default RPCRetryPolicy.
std::unique_ptr<RPCRetryPolicy> DefaultRPCRetryPolicy();

/**
 * Implement a simple exponential backoff policy.
 */
class ExponentialBackoffPolicy : public RPCRetryPolicy {
 public:
  template <typename duration_t1, typename duration_t2>
  ExponentialBackoffPolicy(int maximum_failures, duration_t1 initial_delay,
                           duration_t2 maximum_delay)
      : failure_count_(0),
        maximum_failures_(maximum_failures),
        current_delay_(std::chrono::duration_cast<std::chrono::microseconds>(
            initial_delay)),
        maximum_delay_(std::chrono::duration_cast<std::chrono::microseconds>(
            maximum_delay)) {}

  std::unique_ptr<RPCRetryPolicy> clone() const override;
  bool on_failure(grpc::Status const& status,
                  std::chrono::milliseconds* delay) override;

 private:
  int failure_count_;
  int maximum_failures_;
  std::chrono::microseconds current_delay_;
  std::chrono::microseconds maximum_delay_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // BIGTABLE_CLIENT_RPC_RETRY_POLICY_H_
