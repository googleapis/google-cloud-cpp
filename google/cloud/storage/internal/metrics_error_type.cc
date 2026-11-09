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
#include <string>
#include <string_view>
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

std::string_view ClassifyCurlOrTransportError(Status const& status) {
  StatusCode const code = status.code();
  if (code != StatusCode::kUnavailable && code != StatusCode::kUnknown) {
    return {};
  }
  std::string_view const msg = status.message();
  if (!absl::StrContains(msg, "CURL error [")) return {};
  // `CurlHandle::AsStatus(CURLcode)` maps DNS, connection, and SSL connect
  // errors to `kUnavailable`, while other SSL errors map to `kUnknown`.
  // Gating by `StatusCode` avoids colliding with `CURLMcode` values (5/6/7)
  // formatted identically under `kUnknown`.
  if (code == StatusCode::kUnavailable) {
    if (absl::StrContains(msg, "CURL error [5]=") ||
        absl::StrContains(msg, "CURL error [6]=")) {
      return "DNS_FAILURE";
    }
    if (absl::StrContains(msg, "CURL error [35]=")) return "TLS_FAILURE";
    for (std::string_view const conn_code :
         {"CURL error [7]=", "CURL error [18]=", "CURL error [52]=",
          "CURL error [55]=", "CURL error [56]="}) {
      if (absl::StrContains(msg, conn_code)) return "CONNECTION_ERROR";
    }
    return {};
  }
  for (std::string_view const ssl_code :
       {"CURL error [53]=", "CURL error [54]=", "CURL error [58]=",
        "CURL error [59]=", "CURL error [60]=", "CURL error [64]=",
        "CURL error [77]=", "CURL error [80]=", "CURL error [82]=",
        "CURL error [83]="}) {
    if (absl::StrContains(msg, ssl_code)) return "TLS_FAILURE";
  }
  return {};
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
