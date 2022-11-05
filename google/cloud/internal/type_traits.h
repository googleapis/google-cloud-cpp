// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TYPE_TRAITS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TYPE_TRAITS_H

#include "google/cloud/version.h"
#include <ostream>
#include <type_traits>
#include <utility>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * A helper to detect if expressions are valid and use SFINAE.
 *
 * This class will be introduced in C++17, it makes it easier to write SFINAE
 * expressions, basically you use `void_t< some expression here >` in the SFINAE
 * branch that is trying to test if `some expression here` is valid. If the
 * expression is valid it expands to `void` and your branch continues, if it is
 * invalid it fails and the default SFINAE branch is used.
 *
 * @see https://en.cppreference.com/w/cpp/types/void_t for more details about
 *   this class.
 */
template <typename...>
using void_t = void;

/**
 * A helper to detect if a type is output streamable.
 */
template <typename T>
class IsOStreamable {
 private:
  template <typename U>
  static auto test(int)
      -> decltype(std::declval<std::ostream&>() << std::declval<U>(),
                  std::true_type());
  template <typename>
  static auto test(...) -> std::false_type;

 public:
  static constexpr bool kValue = decltype(test<T>(0))::value;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TYPE_TRAITS_H
