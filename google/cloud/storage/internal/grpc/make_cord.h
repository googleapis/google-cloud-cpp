// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_MAKE_CORD_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_MAKE_CORD_H

#include "google/cloud/version.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

template <typename T>
using IsPayloadType = absl::disjunction<
#if GOOGLE_CLOUD_CPP_CPP_VERSION >= 201703L
    std::is_same<T, std::byte>,
#endif
    std::is_same<T, char>, std::is_same<T, signed char>,
    std::is_same<T, unsigned char>, std::is_same<T, std::uint8_t>>;

/// Create an `absl::Cord`, without copying the data in @p p.
absl::Cord MakeCord(std::string p);

/// Create an `absl::Cord`, without copying the data in @p p.
template <typename T>
absl::Cord MakeCord(std::vector<T> p) {
  static_assert(IsPayloadType<T>::value, "unexpected value type");
  auto holder = std::make_shared<std::vector<T>>(std::move(p));
  auto contents = absl::string_view(
      reinterpret_cast<char const*>(holder->data()), holder->size());
  return absl::MakeCordFromExternal(contents,
                                    [b = std::move(holder)]() mutable {});
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_MAKE_CORD_H
