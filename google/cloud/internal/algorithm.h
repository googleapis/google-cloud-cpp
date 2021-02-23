// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ALGORITHM_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ALGORITHM_H

#include "google/cloud/version.h"
#include <algorithm>
#include <iterator>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

/**
 * Returns true if the container @p c contains the value @p v.
 */
template <typename C, typename V>
bool Contains(C&& c, V&& v) {
  auto const e = std::end(std::forward<C>(c));
  return e != std::find(std::begin(std::forward<C>(c)), e, std::forward<V>(v));
}

/**
 * Returns true if the container @p c contains a value for which @p p is true.
 */
template <typename C, typename UnaryPredicate>
bool ContainsIf(C&& c, UnaryPredicate&& p) {
  auto const e = std::end(std::forward<C>(c));
  return e != std::find_if(std::begin(std::forward<C>(c)), e,
                           std::forward<UnaryPredicate>(p));
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ALGORITHM_H
