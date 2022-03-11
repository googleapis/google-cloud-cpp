// Copyright 2017 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_RPC_RETRY_POLICY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_RPC_RETRY_POLICY_H

#include "google/cloud/bigtable/internal/rpc_policy_parameters.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/internal/retry_policy.h"
#include "google/cloud/status.h"
#include "absl/memory/memory.h"
#include <grpcpp/grpcpp.h>
#include <memory>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

template <typename T>
class CommonRetryPolicy;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
/// An adapter to use `grpc::Status` with the `google::cloud::*Policies`.
struct SafeGrpcRetry {
  static inline bool IsOk(Status const& status) { return status.ok(); }
  static inline bool IsTransientFailure(Status const& status) {
    auto const code = status.code();
    return code == StatusCode::kAborted || code == StatusCode::kUnavailable ||
           google::cloud::internal::IsTransientInternalError(status);
  }
  static inline bool IsPermanentFailure(Status const& status) {
    return !IsOk(status) && !IsTransientFailure(status);
  }

  // TODO(#2344) - remove ::grpc::Status version.
  static inline bool IsOk(grpc::Status const& status) { return status.ok(); }
  static inline bool IsTransientFailure(grpc::Status const& status) {
    return IsTransientFailure(MakeStatusFromRpcError(status));
  }
  static inline bool IsPermanentFailure(grpc::Status const& status) {
    return !IsOk(status) && !IsTransientFailure(status);
  }
};
}  // namespace internal

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
  using RetryableTraits = internal::SafeGrpcRetry;

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
  virtual void Setup(grpc::ClientContext& context) const = 0;

  /**
   * Handle an RPC failure.
   *
   * @return true if the RPC operation should be retried.
   */
  virtual bool OnFailure(Status const& status) = 0;
  // TODO(#2344) - remove ::grpc::Status version.
  virtual bool OnFailure(grpc::Status const& status) = 0;

  static bool IsPermanentFailure(Status const& status) {
    return internal::SafeGrpcRetry::IsPermanentFailure(status);
  }
  // TODO(#2344) - remove ::grpc::Status version.
  static bool IsPermanentFailure(grpc::Status const& status) {
    return internal::SafeGrpcRetry::IsPermanentFailure(status);
  }

  virtual bool IsExhausted() const { return exhausted_; }

 private:
  template <typename T>
  friend class bigtable_internal::CommonRetryPolicy;

  bool exhausted_ = false;
};

/// Return an instance of the default RPCRetryPolicy.
std::unique_ptr<RPCRetryPolicy> DefaultRPCRetryPolicy(
    internal::RPCPolicyParameters defaults);

/**
 * Implement a simple "count errors and then stop" retry policy.
 */
class LimitedErrorCountRetryPolicy : public RPCRetryPolicy {
 public:
  explicit LimitedErrorCountRetryPolicy(int maximum_failures)
      : impl_(maximum_failures) {}

  std::unique_ptr<RPCRetryPolicy> clone() const override;
  void Setup(grpc::ClientContext& context) const override;
  bool OnFailure(Status const& status) override;
  // TODO(#2344) - remove ::grpc::Status version.
  bool OnFailure(grpc::Status const& status) override;
  bool IsExhausted() const override;

 private:
  using Impl = ::google::cloud::internal::LimitedErrorCountRetryPolicy<
      internal::SafeGrpcRetry>;
  Impl impl_;
};

/**
 * Implement a simple "keep trying for this time" retry policy.
 */
class LimitedTimeRetryPolicy : public RPCRetryPolicy {
 public:
  explicit LimitedTimeRetryPolicy(internal::RPCPolicyParameters defaults);
  template <typename DurationT>
  explicit LimitedTimeRetryPolicy(DurationT maximum_duration)
      : impl_(maximum_duration) {}

  std::unique_ptr<RPCRetryPolicy> clone() const override;
  void Setup(grpc::ClientContext& context) const override;
  bool OnFailure(Status const& status) override;
  // TODO(#2344) - remove ::grpc::Status version.
  bool OnFailure(grpc::Status const& status) override;
  bool IsExhausted() const override;

 private:
  using Impl = ::google::cloud::internal::LimitedTimeRetryPolicy<
      internal::SafeGrpcRetry>;
  Impl impl_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

template <typename ReturnType>
class CommonRetryPolicy : public ReturnType {
 public:
  explicit CommonRetryPolicy(std::unique_ptr<bigtable::RPCRetryPolicy> impl)
      : impl_(std::move(impl)) {}
  ~CommonRetryPolicy() override = default;

  std::unique_ptr<ReturnType> clone() const override {
    return absl::make_unique<CommonRetryPolicy>(impl_->clone());
  }
  bool OnFailure(Status const& s) override {
    auto retry = impl_->OnFailure(s);
    if (!retry && !IsPermanentFailure(s)) impl_->exhausted_ = true;
    return retry;
  }
  bool IsExhausted() const override { return impl_->IsExhausted(); }
  bool IsPermanentFailure(Status const& s) const override {
    return bigtable::RPCRetryPolicy::IsPermanentFailure(s);
  }
  void OnFailureImpl() override {}

 private:
  std::unique_ptr<bigtable::RPCRetryPolicy> impl_;
};

template <typename ReturnType>
std::unique_ptr<ReturnType> MakeCommonRetryPolicy(
    std::unique_ptr<bigtable::RPCRetryPolicy> policy) {
  return absl::make_unique<CommonRetryPolicy<ReturnType>>(std::move(policy));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_RPC_RETRY_POLICY_H
