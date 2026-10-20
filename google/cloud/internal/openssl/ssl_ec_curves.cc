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
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENSSL
#include <openssl/ec.h>
#include <openssl/ssl.h>
#endif
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#if OPENSSL_VERSION_NUMBER >= 0x30500000L
namespace {
struct OpenSslDeleter {
  void operator()(STACK_OF(OPENSSL_CSTRING) * ptr) {
    sk_OPENSSL_CSTRING_free(ptr);
  }
  void operator()(SSL_CTX* ptr) { SSL_CTX_free(ptr); }
};
}  // namespace
#endif

std::vector<std::string> AvailableCryptoGroups() {
  std::vector<std::string> groups;
#if GOOGLE_CLOUD_CPP_HAVE_OPENSSL
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
#elif defined(OPENSSL_IS_BORINGSSL) && defined(SSL_GROUP_SECP256R1)
  // BoringSSL reports the names of all the TLS groups it supports. It added
  // `SSL_get_all_group_names()` in the same change as the `SSL_GROUP_*`
  // macros, so we use the latter to detect the former.
  std::vector<char const*> names(SSL_get_all_group_names(nullptr, 0));
  SSL_get_all_group_names(names.data(), names.size());
  for (char const* name : names) {
    if (name != nullptr) groups.emplace_back(name);
  }
#elif defined(OPENSSL_IS_BORINGSSL)
  // Versions of BoringSSL without `SSL_get_all_group_names()` predate support
  // for X25519MLKEM768. Returning an empty list leaves the library's default
  // groups unchanged.
#else   // OpenSSL < 3.5
  size_t const num_curves = EC_get_builtin_curves(nullptr, 0);
  std::vector<EC_builtin_curve> builtin(num_curves);
  EC_get_builtin_curves(builtin.data(), num_curves);
  for (auto const& c : builtin) {
    char const* name = OBJ_nid2sn(c.nid);
    if (name != nullptr) groups.emplace_back(name);
  }
#endif  // OPENSSL_VERSION_NUMBER >= 0x30500000L
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENSSL
  return groups;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
#endif  // _WIN32
