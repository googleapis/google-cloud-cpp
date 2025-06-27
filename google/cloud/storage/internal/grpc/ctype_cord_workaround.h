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
#include "absl/strings/cord.h"
#include "google/storage/v2/storage.pb.h"
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

template <typename P, typename AlwaysVoid = void>
struct HasPublicContent : public std::false_type {};

template <typename P>
struct HasPublicContent<
    P, google::cloud::internal::void_t<decltype(std::declval<P>().content())>>
    : public std::true_type {};

// See /doc/ctype-cord-workarounds for details.
#if GOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND

static_assert(!HasPublicContent<google::storage::v2::ChecksummedData>::value,
              "The workarounds for [ctype = CORD] are enabled, but not needed."
              "\nPlease disable these workarounds as described in:"
              "\n    https://github.com/googleapis/google-cloud-cpp"
              "/blob/main/doc/ctype-cord-workarounds.md"
              "\n");

static_assert(std::is_same<std::string, ContentType>::value,
              "The workarounds for [ctype = CORD] are enabled, but not needed."
              "\nThis will result in compile-time errors."
              "\nPlease disable these workarounds as described in:"
              "\n    https://github.com/googleapis/google-cloud-cpp"
              "/blob/main/doc/ctype-cord-workarounds.md"
              "\n");

// Workaround for older Protobuf versions without `[ctype = CORD]` support.
template <typename Tag, typename Tag::type M>
struct BypassPrivateControl {
  friend constexpr typename Tag::type GetMemberPointer(Tag) { return M; }
};

// A tag to gain access to the `mutable_content()` accessor.
struct GetMutableContentTag {
  using type = std::string* (google::storage::v2::ChecksummedData::*)();
  friend constexpr type GetMemberPointer(GetMutableContentTag);
};

// A tag to gain access to the `content()` accessor.
struct GetContentTag {
  using type =
      std::string const& (google::storage::v2::ChecksummedData::*)() const;
  friend constexpr type GetMemberPointer(GetContentTag);
};

extern template struct BypassPrivateControl<
    GetMutableContentTag,
    &google::storage::v2::ChecksummedData::mutable_content>;
extern template struct BypassPrivateControl<
    GetContentTag, &google::storage::v2::ChecksummedData::content>;

inline std::string const& GetContent(
    google::storage::v2::ChecksummedData const& d) {
  return (d.*GetMemberPointer(GetContentTag{}))();
}

inline void SetMutableContent(google::storage::v2::ChecksummedData& d,
                              std::string value) {
  *(d.*GetMemberPointer(GetMutableContentTag{}))() = std::move(value);
}

inline std::string StealMutableContent(
    google::storage::v2::ChecksummedData& d) {
  return std::move(*(d.*GetMemberPointer(GetMutableContentTag{}))());
}

#else  // GOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND == TRUE

static_assert(
    HasPublicContent<google::storage::v2::ChecksummedData>::value,
    "The workarounds for [ctype = CORD] are disabled, but are required."
    "\nThis will result in compile-time errors"
    "\nPlease enable these workarounds as described in:"
    "\n    https://github.com/googleapis/google-cloud-cpp"
    "/blob/main/doc/ctype-cord-workarounds.md"
    "\n");

// The OSS version of [ctype = CORD] will not support `mutable_content()`. That
// means we can only use `set_content()` when `ContentType == absl::Cord`.
// But for `ContentType == std::string` we really want to use `mutable_*()` to
// steal the contents with zero copy. This extra layer of SFINAE deals with
// this (hopefully the last) layer of complexity.
template <typename T, typename C = std::string>
struct AccessContent {
  static C const& Get(T const& d) {
    return d.content();
    ;
  }

  static void Set(T& d, C value) { *(d.mutable_content()) = std::move(value); }

  static C Steal(T& d) { return std::move(*d.mutable_content()); }
};

template <typename T>
struct AccessContent<T, absl::Cord> {
  static absl::Cord const& Get(T const& d) { return d.content(); }

  static void Set(T& d, absl::Cord value) { d.set_content(std::move(value)); }

  static absl::Cord Steal(T& d) { return d.content(); }
};

inline ContentType const& GetContent(
    google::storage::v2::ChecksummedData const& d) {
  return AccessContent<google::storage::v2::ChecksummedData, ContentType>::Get(
      d);
}

inline void SetMutableContent(google::storage::v2::ChecksummedData& d,
                              ContentType value) {
  AccessContent<google::storage::v2::ChecksummedData, ContentType>::Set(
      d, std::move(value));
}

inline ContentType StealMutableContent(
    google::storage::v2::ChecksummedData& d) {
  return AccessContent<google::storage::v2::ChecksummedData,
                       ContentType>::Steal(d);
}

#endif  // GOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND

template <typename T>
struct AsContentType;

template <>
struct AsContentType<std::string> {
  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  std::string operator()(absl::Cord c) { return static_cast<std::string>(c); }
};

template <>
struct AsContentType<absl::Cord> {
  absl::Cord operator()(absl::Cord c) { return c; }
};

inline void SetContent(google::storage::v2::ChecksummedData& data,
                       absl::Cord contents) {
  SetMutableContent(data, AsContentType<ContentType>{}(std::move(contents)));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_CTYPE_CORD_WORKAROUND_H
