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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_RPC_BACKOFF_POLICY_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_RPC_BACKOFF_POLICY_H_

#include "google/cloud/bigtable/internal/rpc_policy_parameters.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/internal/backoff_policy.h"
#include <grpcpp/grpcpp.h>
#include <chrono>
#include <memory>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Define the interface for controlling how the Bigtable client
 * backsoff from failed RPC operations.
 *
 * The C++ client for Bigtable needs to hide partial and temporary
 * failures from the application.  However, we need to give the users
 * enough flexibility to control how many attempts are made to reissue
 * operations, how often these attempts are executed, and how to
 * signal that an error has occurred.
 *
 * The application provides an instance of this class when the Table
 * (or TableAdmin) object is created.  This instance serves as a
 * prototype to create new RPCBackoffPolicy objects of the same
 * (dynamic) type and with the same initial state.
 */
class RPCBackoffPolicy {
 public:
  virtual ~RPCBackoffPolicy() = default;

  /**
   * Return a new copy of this object.
   *
   * Typically implemented as
   * @code
   *   return std::unique_ptr<RPCRetryPolicy>(new Foo(*this));
   * @endcode
   */
  virtual std::unique_ptr<RPCBackoffPolicy> clone() const = 0;

  /**
   * Update the ClientContext for the next call.
   */
  virtual void Setup(grpc::ClientContext& context) const = 0;

  /**
   * Return the delay after an RPC operation has completed.
   *
   * @return true the delay before trying the operation again.
   * @param status the status returned by the last RPC operation.
   */
  virtual std::chrono::milliseconds OnCompletion(
      google::cloud::Status const& status) = 0;
  // TODO(#2344) - remove ::grpc::Status version.
  virtual std::chrono::milliseconds OnCompletion(grpc::Status const& s) = 0;
};

/// Return an instance of the default RPCBackoffPolicy.
std::unique_ptr<RPCBackoffPolicy> DefaultRPCBackoffPolicy(
    internal::RPCPolicyParameters defaults);

/**
 * Implement a simple exponential backoff policy.
 */
class ExponentialBackoffPolicy : public RPCBackoffPolicy {
 public:
  ExponentialBackoffPolicy(internal::RPCPolicyParameters defaults);
  template <typename duration_t1, typename duration_t2>
  ExponentialBackoffPolicy(duration_t1 initial_delay, duration_t2 maximum_delay)
      : impl_(initial_delay / 2, maximum_delay, 2.0) {}

  std::unique_ptr<RPCBackoffPolicy> clone() const override;
  void Setup(grpc::ClientContext& context) const override;
  std::chrono::milliseconds OnCompletion(
      google::cloud::Status const& status) override;
  // TODO(#2344) - remove ::grpc::Status version.
  std::chrono::milliseconds OnCompletion(grpc::Status const& status) override;

 private:
  using Impl = google::cloud::internal::ExponentialBackoffPolicy;
  Impl impl_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_RPC_BACKOFF_POLICY_H_
