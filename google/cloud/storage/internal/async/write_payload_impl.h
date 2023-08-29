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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_WRITE_PAYLOAD_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_WRITE_PAYLOAD_IMPL_H

#include "google/cloud/storage/async_object_requests.h"
#include "google/cloud/version.h"
#include "absl/meta/type_traits.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// Groups helpers to access implementation details in
/// `storage_experimental::WritePayload`.
struct WritePayloadImpl {
  static storage_experimental::WritePayload Make(absl::Cord impl) {
    return storage_experimental::WritePayload(std::move(impl));
  }
  static absl::Cord GetImpl(storage_experimental::WritePayload const& p) {
    return p.impl_;
  }
};

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

storage_experimental::WritePayload MakeWritePayload(std::string p);
template <typename T,
          typename std::enable_if<IsPayloadType<T>::value, int>::type = 0>
storage_experimental::WritePayload MakeWritePayload(std::vector<T> s) {
  return WritePayloadImpl::Make(MakeCord(std::move(s)));
}

storage_experimental::WritePayload MakeWritePayload(std::vector<std::string> p);
template <typename T,
          typename std::enable_if<IsPayloadType<T>::value, int>::type = 0>
storage_experimental::WritePayload MakeWritePayload(
    std::vector<std::vector<T>> p) {
  auto full = std::accumulate(std::make_move_iterator(p.begin()),
                              std::make_move_iterator(p.end()), absl::Cord(),
                              [](absl::Cord a, std::vector<T> b) {
                                a.Append(MakeCord(std::move(b)));
                                return a;
                              });
  return WritePayloadImpl::Make(std::move(full));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_WRITE_PAYLOAD_IMPL_H
