// Copyright 2023 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_CTYPE_CORD_WORKAROUND_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_CTYPE_CORD_WORKAROUND_H

#include "google/cloud/internal/type_traits.h"
#include "google/cloud/version.h"
#include <google/storage/v2/storage.pb.h>
#include <string>
#include <type_traits>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Discover the `google::storage::v2::ChecksummedData::content()` field type.
// It is harder than usual because the member functions may be private. We
// assume it is `std::string` when it is not queryable, and use SFINAE for
// the other cases.
template <typename C, typename AlwaysVoid = void>
struct ContentTypeImpl {
  using type = std::string;
};

template <typename C>
struct ContentTypeImpl<
    C, google::cloud::internal::void_t<decltype(std::declval<C>().content())>> {
  using type = std::remove_const_t<
      std::remove_reference_t<decltype(std::declval<C>().content())>>;
};

using ContentType =
    typename ContentTypeImpl<google::storage::v2::ChecksummedData>::type;

// Workaround for older Protobuf versions without `[ctype = CORD]` support.
//
// In older versions of Protobuf adding the `[ctype = CORD]` annotation moves
// the field accessors and modifiers to the `private:` section of the generated
// C++ class. That makes the field unusable but there is a workaround. One can
// bypass the C++ member access control mechanism using these tricks.
template <typename T, typename T::type M>
struct BypassPrivateControl {
  friend constexpr typename T::type GetMemberPointer(T) { return M; }
};

// A tag to gain access to the `mutable_content()` accessor.
struct GetMutableContentTag {
  using type = ContentType* (google::storage::v2::ChecksummedData::*)();
  friend constexpr type GetMemberPointer(GetMutableContentTag);
};

extern template struct BypassPrivateControl<
    GetMutableContentTag,
    &google::storage::v2::ChecksummedData::mutable_content>;

struct GetContentTag {
  using type =
      ContentType const& (google::storage::v2::ChecksummedData::*)() const;
  friend constexpr type GetMemberPointer(GetContentTag);
};

extern template struct BypassPrivateControl<
    GetContentTag, &google::storage::v2::ChecksummedData::content>;

// The OSS version of [ctype = CORD] will not support `mutable_content()`. That
// means we can only use `set_content()` when `ContentType == absl::Cord`.
// But for `ContentType == std::string` we really want to use `mutable_*()` to
// steal the contents with zero copy. This extra layer of SFINAE deals with
// this (hopefully the last) layer of complexity.
template <typename T, typename C = typename ContentTypeImpl<T>::type>
struct AccessContent {
  static C const& Get(T const& d) {
    return (d.*GetMemberPointer(GetContentTag{}))();
  }

  static void Set(T& d, C value) {
    *(d.*GetMemberPointer(GetMutableContentTag{}))() = std::move(value);
  }

  static C Steal(T& d) {
    return std::move(*(d.*GetMemberPointer(GetMutableContentTag{}))());
  }
};

template <typename T>
struct AccessContent<T, absl::Cord> {
  static absl::Cord const& Get(T const& d) { return d.content(); }

  static void Set(T& d, absl::Cord value) { d.set_content(std::move(value)); }

  static absl::Cord Steal(T& d) { return d.content(); }
};

inline ContentType const& GetContent(
    google::storage::v2::ChecksummedData const& d) {
  return AccessContent<google::storage::v2::ChecksummedData>::Get(d);
}

inline void SetMutableContent(google::storage::v2::ChecksummedData& d,
                              ContentType value) {
  AccessContent<google::storage::v2::ChecksummedData>::Set(d, std::move(value));
}

inline ContentType StealMutableContent(
    google::storage::v2::ChecksummedData& d) {
  return AccessContent<google::storage::v2::ChecksummedData>::Steal(d);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_CTYPE_CORD_WORKAROUND_H
