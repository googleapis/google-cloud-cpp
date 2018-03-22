// Copyright 2018 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_INTERNAL_UNARY_RPC_UTILS_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_INTERNAL_UNARY_RPC_UTILS_H_

#include "bigtable/client/internal/throw_delegate.h"
#include "bigtable/client/metadata_update_policy.h"
#include "bigtable/client/rpc_backoff_policy.h"
#include "bigtable/client/rpc_retry_policy.h"
#include <thread>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {
namespace noex {
/**
 * Helper functions to make (unary) gRPC calls under the right policies.
 *
 * Many of the gRPC calls made the the Cloud Bigtable C++ client library are
 * wrapped in essentially the same loop:
 *
 * @code
 * clone the policies for the call
 * do {
 *   make rpc call
 *   return if successful
 *   update policies
 * } while(policies allow retry);
 * report failure
 * @endcode
 *
 * The loop is not hard to write, but gets tedious, `CallWithRetry` provides a
 * function that implements this loop.  The code is a bit difficult because the
 * signature of the gRPC functions look like this:
 *
 * @code
 * grpc::Status (StubType::*)(grpc::ClientContext*, Request const&, Response*);
 * @endcode
 *
 * Where `Request` and `Response` are the protos in the gRPC call.
 *
 * @tparam ClientType the type of the client used for the gRPC call.
 */
template <typename ClientType>
struct UnaryRpcUtils {
  /**
   * Extract the StubType from the type returned by ClientType::Stub().
   *
   * This is a meta-function to convert the std::shared_ptr<T> type returned
   * by ClientType::Stub into a plain T.  We follow the STL convention of using
   * `type` for the returned type of this metafunction.
   */
  struct DiscoverStubType {
    using SharedPtr = decltype(std::declval<ClientType>().Stub());

    /// Meta-function to convert shared_ptr<T> into T.  Non-matching case.
    template <typename U>
    struct ExtractSharedType {
      constexpr static bool matched = false;
      using type = void;
    };

    /// Meta-function to convert shared_ptr<T> into T.  Matching case.
    template <typename U>
    struct ExtractSharedType<std::shared_ptr<U>> {
      constexpr static bool matched = true;
      using type = U;
    };

    // Use ExtractSharedType<T> to get the underlying stub.
    static_assert(ExtractSharedType<SharedPtr>::matched,
                  "The type returned by ClientType::Stub() must be a "
                  "shared_ptr<> instantiation");
    using type = typename ExtractSharedType<SharedPtr>::type;
  };

  /// The stub type returned by ClientType::Stub().
  using StubType = typename DiscoverStubType::type;

  /**
   * Determine if @p T is a pointer to member function with the expected
   * signature for `MakeCall()`.
   *
   * This is the generic case, where the type does not match the expected
   * signature.  The class derives from `std::false_type`, so
   * `CheckSignature<T>::%value` is `false`.
   *
   * @tparam T the type to check against the expected signature.
   */
  template <typename T>
  struct CheckSignature : public std::false_type {
    /// Must define ResponseType because it is used in std::enable_if<>.
    using ResponseType = int;
  };

  /**
   * Determine if a type is a pointer to member function with the correct
   * signature for `MakeCall()`.
   *
   * This is the case where the type actually matches the expected signature.
   * This class derives from `std::true_type`, so `CheckSignature<T>::%value` is
   * `true`.  The class also extracts the request and response types used in the
   * implementation of `CallWithRetry()`.
   *
   * @tparam Request the RPC request type.
   * @tparam Response the RPC response type.
   */
  template <typename Request, typename Response>
  struct CheckSignature<grpc::Status (StubType::*)(grpc::ClientContext*,
                                                   Request const&, Response*)>
      : public std::true_type {
    using RequestType = Request;
    using ResponseType = Response;
    using MemberFunctionType = grpc::Status (StubType::*)(grpc::ClientContext*,
                                                          Request const&,
                                                          Response*);
  };

  /**
   * Call a simple unary RPC with retries.
   *
   * Given a pointer to member function in the grpc StubInterface class this
   * generic function calls it with retries until success or until the RPC
   * policies determine that this is an error.
   *
   * We use std::enable_if<> to stop signature errors at compile-time.  The
   * `CheckSignature` meta function returns `false` if given a type that is not
   * a pointer to member with the right signature, that disables this function
   * altogether, and the developer gets a nice-ish error message.
   *
   * @tparam MemberFunction the signature of the member function.
   * @param client the object that holds the gRPC stub.
   * @param rpc_policy the policy controlling what failures are retryable.
   * @param backoff_policy the policy controlling how long to wait before
   *     retrying.
   * @param metadata_update_policy to keep metadata like
   *     x-goog-request-params.
   * @param function the pointer to the member function to call.
   * @param request an initialized request parameter for the RPC.
   * @param error_message include this message in any exception or error log.
   * @return the return parameter from the RPC.
   * @throw std::exception with a description of the last RPC error.
   */
  template <typename MemberFunction>
  // Disable the function if the provided member function does not match the
  // expected signature.  Compilers also emit nice error messages in this case.
  static typename std::enable_if<
      CheckSignature<MemberFunction>::value,
      typename CheckSignature<MemberFunction>::ResponseType>::type
  CallWithRetry(
      ClientType& client, std::unique_ptr<bigtable::RPCRetryPolicy> rpc_policy,
      std::unique_ptr<bigtable::RPCBackoffPolicy> backoff_policy,
      bigtable::MetadataUpdatePolicy const& metadata_update_policy,
      MemberFunction function,
      typename CheckSignature<MemberFunction>::RequestType const& request,
      char const* error_message, grpc::Status& status) {
    return CallWithRetryBorrow(client, *rpc_policy, *backoff_policy,
                               metadata_update_policy, function, request,
                               error_message, status);
  }

  /**
   * Call a simple unary RPC with retries borrowing the RPC policies.
   *
   * This implements `CallWithRetry()`, but does not assume ownership of the RPC
   * policies.  Some RPCs, notably those with pagination, can reuse most of the
   * code in `CallWithRetry()` but must reuse the same policies across several
   * calls.
   *
   * @tparam MemberFunction the signature of the member function.
   * @param client the object that holds the gRPC stub.
   * @param rpc_policy the policy controlling what failures are retryable.
   * @param backoff_policy the policy controlling how long to wait before
   *     retrying.
   * @param metadata_update_policy to keep metadata like
   *     x-goog-request-params.
   * @param function the pointer to the member function to call.
   * @param request an initialized request parameter for the RPC.
   * @param error_message include this message in any exception or error log.
   * @return the return parameter from the RPC.
   * @throw std::exception with a description of the last RPC error.
   */
  template <typename MemberFunction>
  // Disable the function if the provided member function does not match the
  // expected signature.  Compilers also emit nice error messages in this case.
  static typename std::enable_if<
      CheckSignature<MemberFunction>::value,
      typename CheckSignature<MemberFunction>::ResponseType>::type
  CallWithRetryBorrow(
      ClientType& client, bigtable::RPCRetryPolicy& rpc_policy,
      bigtable::RPCBackoffPolicy& backoff_policy,
      bigtable::MetadataUpdatePolicy const& metadata_update_policy,
      MemberFunction function,
      typename CheckSignature<MemberFunction>::RequestType const& request,
      char const* error_message, grpc::Status& status) {
    typename CheckSignature<MemberFunction>::ResponseType response;
    while (true) {
      grpc::ClientContext client_context;
      rpc_policy.setup(client_context);
      backoff_policy.setup(client_context);
      metadata_update_policy.setup(client_context);
      // Call the pointer to member function.
      status =
          ((*client.Stub()).*function)(&client_context, request, &response);
      client.on_completion(status);
      if (status.ok()) {
        break;
      }
      if (not rpc_policy.on_failure(status)) {
        status = grpc::Status(status.error_code(), error_message);
        break;
      }
      auto delay = backoff_policy.on_completion(status);
      std::this_thread::sleep_for(delay);
    }
    return response;
  }

  /**
   * Call a simple unary RPC with no retry.
   *
   * Given a pointer to member function in the grpc StubInterface class this
   * generic function calls it with retries until success or until the RPC
   * policies determine that this is an error.
   *
   * We use std::enable_if<> to stop signature errors at compile-time.  The
   * `CheckSignature` meta function returns `false` if given a type that is not
   * a pointer to member with the right signature, that disables this function
   * altogether, and the developer gets a nice-ish error message.
   *
   * @tparam MemberFunction the signature of the member function.
   * @param client the object that holds the gRPC stub.
   * @param rpc_policy the policy to control timeouts.
   * @param metadata_update_policy to keep metadata like
   *     x-goog-request-params.
   * @param function the pointer to the member function to call.
   * @param request an initialized request parameter for the RPC.
   * @param error_message include this message in any exception or error log.
   * @return the return parameter from the RPC.
   * @throw std::exception with a description of the last RPC error.
   */
  template <typename MemberFunction>
  // Disable the function if the provided member function does not match the
  // expected signature.  Compilers also emit nice error messages in this case.
  static typename std::enable_if<
      CheckSignature<MemberFunction>::value,
      typename CheckSignature<MemberFunction>::ResponseType>::type
  CallWithoutRetry(
      ClientType& client, std::unique_ptr<bigtable::RPCRetryPolicy> rpc_policy,
      bigtable::MetadataUpdatePolicy const& metadata_update_policy,
      MemberFunction function,
      typename CheckSignature<MemberFunction>::RequestType const& request,
      char const* error_message, grpc::Status& status) {
    typename CheckSignature<MemberFunction>::ResponseType response;

    grpc::ClientContext client_context;

    // Policies can set timeouts so allowing them to update context
    rpc_policy->setup(client_context);
    metadata_update_policy.setup(client_context);
    // Call the pointer to member function.
    status = ((*client.Stub()).*function)(&client_context, request, &response);
    client.on_completion(status);

    return response;
  }
};

}  // namespace noex

template <typename ClientType>
struct UnaryRpcUtils {
  using StubType =
      typename noex::UnaryRpcUtils<ClientType>::DiscoverStubType::type;

  template <typename MemberFunction>
  static typename std::enable_if<
      noex::UnaryRpcUtils<ClientType>::template CheckSignature<
          MemberFunction>::value,
      typename noex::UnaryRpcUtils<ClientType>::template CheckSignature<
          MemberFunction>::ResponseType>::type
  CallWithRetryBorrow(
      ClientType& client, bigtable::RPCRetryPolicy& rpc_policy,
      bigtable::RPCBackoffPolicy& backoff_policy,
      bigtable::MetadataUpdatePolicy const& metadata_update_policy,
      MemberFunction function,
      typename noex::UnaryRpcUtils<ClientType>::template CheckSignature<
          MemberFunction>::RequestType const& request,
      char const* error_message) {
    grpc::Status status;
    auto response = noex::UnaryRpcUtils<ClientType>::CallWithRetryBorrow(
        client, rpc_policy, backoff_policy, metadata_update_policy, function,
        request, error_message, status);
    if (not status.ok()) {
      RaiseRpcError(status, status.error_message());
    }
    return response;
  }

  template <typename MemberFunction>
  static typename std::enable_if<
      noex::UnaryRpcUtils<ClientType>::template CheckSignature<
          MemberFunction>::value,
      typename noex::UnaryRpcUtils<ClientType>::template CheckSignature<
          MemberFunction>::ResponseType>::type
  CallWithRetry(
      ClientType& client, std::unique_ptr<bigtable::RPCRetryPolicy> rpc_policy,
      std::unique_ptr<bigtable::RPCBackoffPolicy> backoff_policy,
      bigtable::MetadataUpdatePolicy const& metadata_update_policy,
      MemberFunction function,
      typename noex::UnaryRpcUtils<ClientType>::template CheckSignature<
          MemberFunction>::RequestType const& request,
      char const* error_message) {
    return CallWithRetryBorrow(client, *rpc_policy, *backoff_policy,
                               metadata_update_policy, function, request,
                               error_message);
  }

  template <typename MemberFunction>
  static typename std::enable_if<
      noex::UnaryRpcUtils<ClientType>::template CheckSignature<
          MemberFunction>::value,
      typename noex::UnaryRpcUtils<ClientType>::template CheckSignature<
          MemberFunction>::ResponseType>::type
  CallWithoutRetry(
      ClientType& client, std::unique_ptr<bigtable::RPCRetryPolicy> rpc_policy,
      bigtable::MetadataUpdatePolicy const& metadata_update_policy,
      MemberFunction function,
      typename noex::UnaryRpcUtils<ClientType>::template CheckSignature<
          MemberFunction>::RequestType const& request,
      char const* error_message) {
    grpc::Status status;
    auto response = noex::UnaryRpcUtils<ClientType>::CallWithoutRetry(
        client, std::move(rpc_policy), std::move(metadata_update_policy),
        function, request, error_message, status);
    if (not status.ok()) {
      RaiseRpcError(status, status.error_message());
    }
    return response;
  }
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_INTERNAL_UNARY_RPC_UTILS_H_
