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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_INTERNAL_CONJUNCTION_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_INTERNAL_CONJUNCTION_H_

#include "bigtable/client/version.h"

#include <type_traits>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

// TODO(#108) - use std::conjunction<> if available.
/// A metafunction to fold && across a list of types, empty list case.
template <typename...>
struct conjunction : std::true_type {};

/// A metafunction to fold && across a list of types, a list with a single
/// element.
template <typename B1>
struct conjunction<B1> : B1 {};

/// A metafunction to fold && across a list of types.
template <typename B1, typename... Bn>
struct conjunction<B1, Bn...>
    : std::conditional<bool(B1::value), conjunction<Bn...>, B1>::type {};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_INTERNAL_CONJUNCTION_H_
