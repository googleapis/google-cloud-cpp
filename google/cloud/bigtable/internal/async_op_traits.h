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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_OP_TRAITS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_OP_TRAITS_H_

#include "google/cloud/bigtable/version.h"
#include "google/cloud/internal/invoke_result.h"
#include <type_traits>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

/**
 * SFINAE detector whether class `C` has a `AccumulatedResult` member function.
 *
 * Catch-all, negative branch.
 */
template <typename C, typename M = void>
struct HasAccumulatedResult : public std::false_type {};

/**
 * SFINAE detector whether class `C` has a `AccumulatedResult` member function.
 *
 * Positive branch.
 */
template <typename C>
struct HasAccumulatedResult<
    C, google::cloud::internal::void_t<decltype(&C::AccumulatedResult)>>
    : public std::true_type {};

/**
 * SFINAE detector whether class `C` has a `Start<Functor>` member function.
 *
 * Catch-all, negative branch.
 */
template <typename C, typename Functor, typename M = void>
struct HasStart : public std::false_type {};

/**
 * SFINAE detector whether class `C` has a `Start<Functor>` member function.
 *
 * Positive branch.
 */
template <typename C, typename Functor>
struct HasStart<
    C, Functor,
    google::cloud::internal::void_t<decltype(&C::template Start<Functor>)>>
    : public std::true_type {};

/**
 * SFINAE detector whether class `C` has a `Cancel` member function.
 *
 * Catch-all, negative branch.
 */
template <typename C, typename M = void>
struct HasCancel : public std::false_type {};

/**
 * SFINAE detector whether class `C` has a `Cancel` member function.
 *
 * Positive branch.
 */
template <typename C>
struct HasCancel<C, google::cloud::internal::void_t<decltype(&C::Cancel)>>
    : public std::true_type {};

/**
 * SFINAE detector whether class `C` has a `WaitPeriod` member function.
 *
 * Catch-all, negative branch.
 */
template <typename C, typename M = void>
struct HasWaitPeriod : public std::false_type {};

/**
 * SFINAE detector whether class `C` has a `WaitPeriod` member function.
 *
 * Positive branch.
 */
template <typename C>
struct HasWaitPeriod<C,
                     google::cloud::internal::void_t<decltype(&C::WaitPeriod)>>
    : public std::true_type {};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_OP_TRAITS_H_
