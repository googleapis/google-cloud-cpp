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

#include "google/cloud/internal/ssl_ec_curves.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/make_status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_join.h"
#include <curl/curl.h>
#include <algorithm>
#include <utility>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

// libcurl encodes version Ma.Mi.Pa as (Ma << 16) | (Mi << 8) | Pa.
// CURLOPT_SSL_EC_CURVES was introduced in libcurl version 7.73.0.
constexpr unsigned int kMinimumLibcurlVersionForEcCurves =
    (7U << 16) | (73U << 8) | 0U;

// `curl_version_info()` is not thread-safe as libcurl constructs version
// string details in internal static buffers without locks. Using a block-scope
// static ensures thread-safe initialization exactly once via C++11 magic
// statics, preventing data races when multiple threads create REST clients.
bool SupportsSslEcCurves() {
  static bool const kSupports = [] {
    auto* vinfo = curl_version_info(CURLVERSION_NOW);
    return vinfo != nullptr &&
           vinfo->version_num >= kMinimumLibcurlVersionForEcCurves;
  }();
  return kSupports;
}

}  // namespace

StatusOr<std::string> PrependPqcEcCurve(std::vector<std::string> groups,
                                        bool supports_ssl_ec_curves) {
  if (internal::GetEnv("GOOGLE_CLOUD_CPP_DISABLE_PQC")) {
    return absl::StrJoin(groups, ":");
  }
  if (!supports_ssl_ec_curves) {
    return internal::UnavailableError(
        "libcurl version 7.73.0 or later is required to set SSL EC curves.");
  }

  std::string_view constexpr kTarget = "X25519MLKEM768";
  auto it = std::find_if(groups.begin(), groups.end(),
                         [&kTarget](std::string const& s) {
                           return absl::EqualsIgnoreCase(s, kTarget);
                         });

  if (it == groups.end()) {
    return internal::UnavailableError(
        "X25519MLKEM768 is not supported by the underlying crypto library.");
  }

  std::string exact_name = *it;
  groups.erase(it);
  groups.insert(groups.begin(), std::move(exact_name));

  return absl::StrJoin(groups, ":");
}

StatusOr<std::string> GetPqcEcCurves() {
  return PrependPqcEcCurve(AvailableCryptoGroups(), SupportsSslEcCurves());
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
