// Copyright 2019 Google LLC
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

#include "google/cloud/internal/sha256_hmac.h"
#ifdef _WIN32
#include <Windows.h>
#include <wincrypt.h>
#else
#include <openssl/evp.h>
#include <openssl/hmac.h>
#endif  // _WIN32
#include <algorithm>
#include <array>
#include <type_traits>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

static_assert(std::is_same<std::uint8_t, unsigned char>::value,
              "When `std::uint8_t` exists it must be `unsigned char`");

template <typename T>
Sha256Type Sha256HmacImpl(absl::Span<T const> key, unsigned char const* data,
                          std::size_t count) {
  Sha256Type hash;
#ifdef _WIN32
// Workaround missing macros in MinGW-w64:
//     https://github.com/mingw-w64/mingw-w64/issues/49
#ifndef BCRYPT_HMAC_SHA256_ALG_HANDLE
#define BCRYPT_HMAC_SHA256_ALG_HANDLE ((BCRYPT_ALG_HANDLE)0x000000b1)
#endif
  BCryptHash(BCRYPT_HMAC_SHA256_ALG_HANDLE,
             reinterpret_cast<PUCHAR>(const_cast<T*>(key.data())),
             static_cast<ULONG>(key.size()), const_cast<PUCHAR>(data),
             static_cast<ULONG>(count), hash.data(),
             static_cast<ULONG>(hash.size()));
#else
  static_assert(EVP_MAX_MD_SIZE >= Sha256Type().size(),
                "EVP_MAX_MD_SIZE is too small");
  std::array<unsigned char, EVP_MAX_MD_SIZE> digest;

  unsigned int size = 0;
  HMAC(EVP_sha256(), key.data(), static_cast<unsigned int>(key.size()), data,
       count, digest.data(), &size);
  std::copy_n(digest.begin(), std::min<std::size_t>(size, hash.size()),
              hash.begin());
#endif
  return hash;
}
}  // namespace

Sha256Type Sha256Hmac(std::string const& key, absl::Span<char const> data) {
  return Sha256HmacImpl(absl::Span<char const>(key),
                        reinterpret_cast<unsigned char const*>(data.data()),
                        data.size());
}

Sha256Type Sha256Hmac(std::string const& key,
                      absl::Span<std::uint8_t const> data) {
  return Sha256HmacImpl(absl::Span<char const>(key),
                        reinterpret_cast<unsigned char const*>(data.data()),
                        data.size());
}

Sha256Type Sha256Hmac(Sha256Type const& key, absl::Span<char const> data) {
  return Sha256HmacImpl(absl::Span<std::uint8_t const>(key),
                        reinterpret_cast<unsigned char const*>(data.data()),
                        data.size());
}

Sha256Type Sha256Hmac(Sha256Type const& key,
                      absl::Span<std::uint8_t const> data) {
  return Sha256HmacImpl(absl::Span<std::uint8_t const>(key),
                        reinterpret_cast<unsigned char const*>(data.data()),
                        data.size());
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
