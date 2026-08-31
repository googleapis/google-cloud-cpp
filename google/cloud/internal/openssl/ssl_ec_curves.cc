// Copyright 2026 Google LLC
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

#ifndef _WIN32
#include "google/cloud/internal/ssl_ec_curves.h"
#include <openssl/ec.h>
#include <openssl/ssl.h>
#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#if OPENSSL_VERSION_NUMBER >= 0x30500000L || defined(OPENSSL_IS_BORINGSSL)
namespace {
struct OpenSslDeleter {
#if OPENSSL_VERSION_NUMBER >= 0x30500000L
  void operator()(STACK_OF(OPENSSL_CSTRING) * ptr) {
    sk_OPENSSL_CSTRING_free(ptr);
  }
#endif
  void operator()(SSL_CTX* ptr) { SSL_CTX_free(ptr); }
};
}  // namespace
#endif

std::vector<std::string> AvailableCryptoGroups() {
  std::vector<std::string> groups;
#if OPENSSL_VERSION_NUMBER >= 0x30500000L
  auto ctx =
      std::unique_ptr<SSL_CTX, OpenSslDeleter>(SSL_CTX_new(TLS_method()));
  if (!ctx) return groups;
  auto names = std::unique_ptr<STACK_OF(OPENSSL_CSTRING), OpenSslDeleter>(
      sk_OPENSSL_CSTRING_new_null());
  if (!names) return groups;
  if (SSL_CTX_get0_implemented_groups(ctx.get(), 1, names.get()) > 0) {
    int const num_names = sk_OPENSSL_CSTRING_num(names.get());
    for (int i = 0; i < num_names; ++i) {
      char const* name = sk_OPENSSL_CSTRING_value(names.get(), i);
      if (name != nullptr) groups.emplace_back(name);
    }
  }
#else  // OPENSSL_VERSION_NUMBER < 0x30500000L
  std::vector<std::string> builtin_groups;
  size_t const num_curves = EC_get_builtin_curves(nullptr, 0);
  std::vector<EC_builtin_curve> builtin(num_curves);
  EC_get_builtin_curves(builtin.data(), num_curves);
  for (auto const& c : builtin) {
    char const* name = OBJ_nid2sn(c.nid);
    if (name != nullptr) builtin_groups.emplace_back(name);
  }
#ifdef OPENSSL_IS_BORINGSSL
  // "secp224r1" can be included in the builtin curves, but it results in a
  // "CURL error [59]=Couldn't use specified SSL cipher" error.
  for (auto const& curve : builtin_groups) {
    if (curve == "secp224r1") continue;
    groups.emplace_back(curve);
  }

  // Unlike OpenSSL, BoringSSL does not provide a way to query the available
  // curves, so we have to probe the library using a temporary SSL_CTX.
  std::array<std::string_view, 3> constexpr kCandidates{
      "X25519",
      "X25519MLKEM768",
      "X25519Kyber768Draft00",
  };

  auto ctx =
      std::unique_ptr<SSL_CTX, OpenSslDeleter>(SSL_CTX_new(TLS_method()));
  if (!ctx) return groups;
  for (auto const& curve : kCandidates) {
    // SSL_CTX_set1_curves_list returns 1 if the curve(s) are
    // recognized/supported
    if (SSL_CTX_set1_curves_list(ctx.get(), curve.data()) == 1) {
      groups.emplace_back(curve);
    }
  }
#else
  groups = std::move(builtin_groups);
#endif  // OPENSSL_IS_BORINGSSL
#endif  // OPENSSL_VERSION_NUMBER >= 0x30500000L
  return groups;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
#endif  // _WIN32
