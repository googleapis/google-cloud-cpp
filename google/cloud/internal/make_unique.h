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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_MAKE_UNIQUE_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_MAKE_UNIQUE_H_

#include "google/cloud/version.h"
#include <memory>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
/**
 * A naive implementation of std::make_unique for C++11.
 *
 * We missed std::make_unique<T>, but have to limit ourselves to C++11 and this
 * is a C++14 feature.  This implementation is super naive and does not work for
 * array types.
 *
 * @tparam T The type of the object to wrap in a std::unique_ptr<>
 * @tparam Args the type of the constructor arguments.
 * @param a the constructor arguments.
 * @return a std::unique_ptr<T> initialized with a new T object constructed with
 *     the given parameters.
 */
template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... a) {
  static_assert(
      !std::is_array<T>::value,
      "Sorry, array types not supported in bigtable::internal::make_unique<T>");
  return std::unique_ptr<T>(new T(std::forward<Args>(a)...));
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_MAKE_UNIQUE_H_
