// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_FWD_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_FWD_H

#include "google/cloud/version.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
// Forward declare the promise type so we can write some helpers.
template <typename R>
// NOLINTNEXTLINE(readability-identifier-naming)
class promise;

// Forward declare the future type so we can write some helpers.
template <typename R>
// NOLINTNEXTLINE(readability-identifier-naming)
class future;

// Forward declare the specializations for references.
template <typename R>
// NOLINTNEXTLINE(readability-identifier-naming)
class promise<R&>;
template <typename R>
// NOLINTNEXTLINE(readability-identifier-naming)
class future<R&>;

// Forward declare the specialization for `void`.
template <>
// NOLINTNEXTLINE(readability-identifier-naming)
class promise<void>;
template <>
// NOLINTNEXTLINE(readability-identifier-naming)
class future<void>;
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_FWD_H
