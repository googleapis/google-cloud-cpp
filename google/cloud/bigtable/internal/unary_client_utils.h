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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_UNARY_CLIENT_UTILS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_UNARY_CLIENT_UTILS_H

#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/rpc_backoff_policy.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/grpc_utils/grpc_error_delegate.h"
#include <thread>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {
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
struct UnaryClientUtils {
  /**
   * A metafunction to determine if @p F is a pointer to member function with
   * the signature expected by `MakeCall()`. If it is, member aliases for
   * `RequestType` and `ResponseType` will be defined to the types used in `F`.
   *
   * @tparam F the type to check against the expected signature.
   */
  template <typename F>
  struct Signature {};

  // Partial specialization for the `Signature` metafunction.
  template <typename Request, typename Response>
  struct Signature<grpc::Status (ClientType::*)(grpc::ClientContext*,
                                                Request const&, Response*)> {
    using RequestType = Request;
    using ResponseType = Response;
  };

  /**
   * Call a simple unary RPC with retries.
   *
   * Given a pointer to member function in the grpc StubInterface class this
   * generic function calls it with retries until success or until the RPC
   * policies determine that this is an error.
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
  static typename Signature<MemberFunction>::ResponseType MakeCall(
      ClientType& client, std::unique_ptr<bigtable::RPCRetryPolicy> rpc_policy,
      std::unique_ptr<bigtable::RPCBackoffPolicy> backoff_policy,
      bigtable::MetadataUpdatePolicy const& metadata_update_policy,
      MemberFunction function,
      typename Signature<MemberFunction>::RequestType const& request,
      char const* error_message, grpc::Status& status, bool retry_on_failure) {
    return MakeCall(client, *rpc_policy, *backoff_policy,
                    metadata_update_policy, function, request, error_message,
                    status, retry_on_failure);
  }

  /**
   * Call a simple unary RPC with retries borrowing the RPC policies.
   *
   * This implements `MakeCall()`, but does not assume ownership of the RPC
   * policies.  Some RPCs, notably those with pagination, can reuse most of the
   * code in `MakeCall()` but must reuse the same policies across several
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
  static typename Signature<MemberFunction>::ResponseType MakeCall(
      ClientType& client, bigtable::RPCRetryPolicy& rpc_policy,
      bigtable::RPCBackoffPolicy& backoff_policy,
      bigtable::MetadataUpdatePolicy const& metadata_update_policy,
      MemberFunction function,
      typename Signature<MemberFunction>::RequestType const& request,
      char const* error_message, grpc::Status& status, bool retry_on_failure) {
    typename Signature<MemberFunction>::ResponseType response;
    do {
      grpc::ClientContext client_context;
      rpc_policy.Setup(client_context);
      backoff_policy.Setup(client_context);
      metadata_update_policy.Setup(client_context);
      // Call the pointer to member function.
      status = (client.*function)(&client_context, request, &response);
      if (status.ok()) {
        break;
      }
      if (!rpc_policy.OnFailure(status)) {
        std::string full_message = error_message;
        full_message += "(" + metadata_update_policy.value() + ") ";
        full_message += status.error_message();
        status = grpc::Status(status.error_code(), full_message,
                              status.error_details());
        break;
      }
      auto delay = backoff_policy.OnCompletion(status);
      std::this_thread::sleep_for(delay);
    } while (retry_on_failure);
    return response;
  }

  /**
   * Call a simple unary RPC with no retry.
   *
   * Given a pointer to member function in the grpc StubInterface class this
   * generic function calls it with retries until success or until the RPC
   * policies determine that this is an error.
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
  static typename Signature<MemberFunction>::ResponseType MakeNonIdemponentCall(
      ClientType& client, std::unique_ptr<bigtable::RPCRetryPolicy> rpc_policy,
      bigtable::MetadataUpdatePolicy const& metadata_update_policy,
      MemberFunction function,
      typename Signature<MemberFunction>::RequestType const& request,
      char const* error_message, grpc::Status& status) {
    typename Signature<MemberFunction>::ResponseType response;

    grpc::ClientContext client_context;

    // Policies can set timeouts so allowing them to update context
    rpc_policy->Setup(client_context);
    metadata_update_policy.Setup(client_context);
    // Call the pointer to member function.
    status = (client.*function)(&client_context, request, &response);

    if (!status.ok()) {
      std::string full_message = error_message;
      full_message += "(" + metadata_update_policy.value() + ") ";
      full_message += status.error_message();
      status = grpc::Status(status.error_code(), full_message,
                            status.error_details());
    }
    return response;
  }
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_UNARY_CLIENT_UTILS_H
