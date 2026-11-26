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

#include "google/cloud/storage/internal/metrics_error_type.h"
#include "absl/strings/match.h"
#include <charconv>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

bool IsChecksumMismatch(Status const& status) {
  StatusCode const code = status.code();
  if (code != StatusCode::kDataLoss && code != StatusCode::kInvalidArgument) {
    return false;
  }
  std::string_view const msg = status.message();
  if (absl::StrContains(msg, "mismatched checksums detected") ||
      absl::StrContains(msg, "(): mismatched hashes in ")) {
    return true;
  }
  return absl::StrContainsIgnoreCase(msg, "mismatched crc32c") ||
         absl::StrContainsIgnoreCase(msg, "mismatched md5") ||
         absl::StrContainsIgnoreCase(msg,
                                     "crc32c mismatch: server computed:") ||
         absl::StrContainsIgnoreCase(msg, "md5 mismatch: server computed:") ||
         ((absl::StrContainsIgnoreCase(msg, "crc32c") ||
           absl::StrContainsIgnoreCase(msg, "md5")) &&
          (absl::StrContainsIgnoreCase(msg, "doesn't match calculated") ||
           absl::StrContainsIgnoreCase(msg, "does not match calculated")));
}

bool IsClientTimeout(Status const& status) {
  if (status.code() != StatusCode::kDeadlineExceeded) return false;
  std::string_view const msg = status.message();
  return status.error_info().domain() == "gcloud-cpp" ||
         absl::StrContains(msg, "Deadline exceeded waiting for data in ") ||
         (absl::StrContains(msg, "timeout [") &&
          absl::StrContains(msg, "] while waiting for ")) ||
         absl::StrContains(msg, "CURL error [28]=");
}

std::optional<int> ExtractCurlErrorCode(std::string_view msg) {
  static constexpr std::string_view kPrefix = "CURL error [";
  std::size_t const pos = msg.find(kPrefix);
  if (pos == std::string_view::npos) return std::nullopt;
  std::string_view const tail = msg.substr(pos + kPrefix.size());
  std::size_t const close = tail.find("]=");
  if (close == std::string_view::npos) return std::nullopt;
  int curl_code = 0;
  char const* const end = tail.data() + close;
  std::from_chars_result const res =
      std::from_chars(tail.data(), end, curl_code);
  if (res.ec != std::errc{} || res.ptr != end) return std::nullopt;
  return curl_code;
}

std::string_view ClassifyCurlOrTransportError(Status const& status) {
  StatusCode const code = status.code();
  if (code != StatusCode::kUnavailable && code != StatusCode::kUnknown) {
    return {};
  }
  std::optional<int> const curl_code = ExtractCurlErrorCode(status.message());
  if (!curl_code.has_value()) return {};
  // `CurlHandle::AsStatus(CURLcode)` maps DNS, connection, and SSL connect
  // (plus newer default-case) errors to `kUnavailable`, while other SSL errors
  // map to `kUnknown`. Gating by `StatusCode` avoids colliding with `CURLMcode`
  // values (5/6/7) formatted identically under `kUnknown`.
  if (code == StatusCode::kUnavailable) {
    switch (*curl_code) {
      case 5:  // CURLE_COULDNT_RESOLVE_PROXY
      case 6:  // CURLE_COULDNT_RESOLVE_HOST
        return "DNS_FAILURE";
      case 35:  // CURLE_SSL_CONNECT_ERROR
      case 66:  // CURLE_SSL_ENGINE_INITFAILED
      case 90:  // CURLE_SSL_PINNEDPUBKEYNOTMATCH
      case 91:  // CURLE_SSL_INVALIDCERTSTATUS
      case 98:  // CURLE_SSL_CLIENTCERT
        return "TLS_FAILURE";
      case 7:   // CURLE_COULDNT_CONNECT
      case 18:  // CURLE_PARTIAL_FILE
      case 52:  // CURLE_GOT_NOTHING
      case 55:  // CURLE_SEND_ERROR
      case 56:  // CURLE_RECV_ERROR
      case 65:  // CURLE_SEND_FAIL_REWIND
      case 89:  // CURLE_NO_CONNECTION_AVAILABLE
      case 96:  // CURLE_QUIC_CONNECT_ERROR
        return "CONNECTION_ERROR";
      default:
        return {};
    }
  }
  switch (*curl_code) {
    case 51:  // CURLE_PEER_FAILED_VERIFICATION (libcurl < 7.62)
    case 53:  // CURLE_SSL_ENGINE_NOTFOUND
    case 54:  // CURLE_SSL_ENGINE_SETFAILED
    case 58:  // CURLE_SSL_CERTPROBLEM
    case 59:  // CURLE_SSL_CIPHER
    case 60:  // CURLE_PEER_FAILED_VERIFICATION
    case 64:  // CURLE_USE_SSL_FAILED
    case 77:  // CURLE_SSL_CACERT_BADFILE
    case 80:  // CURLE_SSL_SHUTDOWN_FAILED
    case 82:  // CURLE_SSL_CRL_BADFILE
    case 83:  // CURLE_SSL_ISSUER_ERROR
      return "TLS_FAILURE";
    default:
      return {};
  }
}

std::string_view ClassifyHttpMetadataCode(Status const& status) {
  // The REST layer coalesces HTTP 408/429/500/504 into `kUnavailable`.
  // `http_status_code` is attached in `ErrorInfo` metadata for empty or
  // structured JSON error bodies (see `ParseJsonError()`), so non-JSON proxy
  // responses fall through as `UNAVAILABLE`.
  if (status.code() != StatusCode::kUnavailable) return {};
  std::unordered_map<std::string, std::string> const& metadata =
      status.error_info().metadata();
  auto const it = metadata.find("http_status_code");
  if (it == metadata.end()) return {};
  if (it->second == "429") return "RESOURCE_EXHAUSTED";
  if (it->second == "408" || it->second == "504") return "DEADLINE_EXCEEDED";
  if (it->second == "500") return "INTERNAL";
  return {};
}

}  // namespace

std::string ComputeErrorType(Status const& status) {
  if (status.ok()) return "OK";
  if (IsChecksumMismatch(status)) return "CHECKSUM_MISMATCH";
  if (IsClientTimeout(status)) return "TIMEOUT";
  if (std::string_view const http_code = ClassifyHttpMetadataCode(status);
      !http_code.empty()) {
    return std::string(http_code);
  }
  if (std::string_view const transport = ClassifyCurlOrTransportError(status);
      !transport.empty()) {
    return std::string(transport);
  }
  return StatusCodeToString(status.code());
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
