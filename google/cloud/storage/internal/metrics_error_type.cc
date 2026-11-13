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
#include <string_view>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

bool IsChecksumMismatch(Status const& status) {
  auto const code = status.code();
  if (code != StatusCode::kDataLoss && code != StatusCode::kInvalidArgument) {
    return false;
  }
  std::string_view const msg = status.message();
  if (absl::StrContains(msg, "mismatched checksums detected") ||
      absl::StrContains(msg, "(): mismatched hashes in ")) {
    return true;
  }
  return (absl::StrContainsIgnoreCase(msg, "crc32c") ||
          absl::StrContainsIgnoreCase(msg, "md5")) &&
         absl::StrContainsIgnoreCase(msg, "match");
}

std::string_view ClassifyHttpMetadataCode(Status const& status) {
  if (status.code() != StatusCode::kUnavailable) return {};
  auto const& metadata = status.error_info().metadata();
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
  if (std::string_view const http_code = ClassifyHttpMetadataCode(status);
      !http_code.empty()) {
    return std::string(http_code);
  }
  return StatusCodeToString(status.code());
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
