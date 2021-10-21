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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RAW_CLIENT_WRAPPER_UTILS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RAW_CLIENT_WRAPPER_UTILS_H

#include "google/cloud/storage/internal/raw_client.h"
#include "google/cloud/storage/version.h"
#include <type_traits>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Defines types to wrap `RawClient` function calls.
 *
 * We have a couple of classes that basically wrap every function in `RawClient`
 * with some additional behavior (`LoggingClient` logs every call, and
 * `RetryClient` retries every call).  Instead of hand-coding every wrapped
 * function we use a helper to wrap it, and in turn those helpers use the
 * meta-functions defined here.
 */
namespace raw_client_wrapper_utils {

/**
 * Metafunction to determine if @p F is a pointer to member function with the
 * expected signature for a `RawClient` member function.
 *
 * This is the generic case, where the type does not match the expected
 * signature and so member type aliases do not exist.
 *
 * @tparam F the type to check against the expected signature.
 */
template <typename F>
struct Signature {};

/**
 * Partial specialization for the above `Signature` metafunction.
 *
 * This is the case where the type actually matches the expected signature. The
 * class also extracts the request and response types used in the
 * implementation of `CallWithRetry()`.
 *
 * @tparam Request the RPC request type.
 * @tparam Response the RPC response type.
 */
template <typename Request, typename Response>
struct Signature<StatusOr<Response> (
    google::cloud::storage::internal::RawClient::*)(Request const&)> {
  using RequestType = Request;
  using ReturnType = StatusOr<Response>;
};

}  // namespace raw_client_wrapper_utils
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RAW_CLIENT_WRAPPER_UTILS_H
