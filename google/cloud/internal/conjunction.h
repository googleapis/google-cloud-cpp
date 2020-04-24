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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CONJUNCTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CONJUNCTION_H

#include "google/cloud/version.h"
#include <type_traits>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

/// A metafunction that folds && across a list of types, the specialization for
/// an empty list.
template <typename...>
struct conjunction : std::true_type {};

/// A metafunction that folds && across a list of types, the specialization for
/// a single element.
template <typename B1>
struct conjunction<B1> : B1 {};

/// A metafunction that folds && across a list of types.
template <typename B1, typename... Bn>
struct conjunction<B1, Bn...>
    : std::conditional<bool(B1::value), conjunction<Bn...>, B1>::type {};

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CONJUNCTION_H
