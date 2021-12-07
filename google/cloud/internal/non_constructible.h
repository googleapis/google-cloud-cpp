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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_NON_CONSTRUCTIBLE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_NON_CONSTRUCTIBLE_H

#include "google/cloud/version.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * A type that is not constructible.
 *
 * This type can be used to disallow the instantiation of an object. We might
 * use it, for example, to disambiguate overloaded functions that are passed an
 * empty initializer list.
 */
struct NonConstructible {
  NonConstructible() = delete;
  NonConstructible(const NonConstructible&) = delete;
  NonConstructible(NonConstructible&&) = delete;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_NON_CONSTRUCTIBLE_H
