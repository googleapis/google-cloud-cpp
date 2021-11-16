// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/status.h"
#include <sstream>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {
std::string StatusWhat(Status const& status) {
  std::ostringstream os;
  os << status;
  return std::move(os).str();
}
}  // namespace

namespace internal {

// Sets the given `payload`, indexed by the given `key`, on the given `Status`,
// IFF the status is not OK. Payloads are considered in equality comparisons.
// The keyspace used here is separate from other keyspaces (e.g.,
// `absl::Status`), so we only need to coordinate keys with ourselves.
void SetPayload(Status& s, std::string key, std::string payload) {
  if (!s.ok()) s.payload_[std::move(key)] = std::move(payload);
}

// Returns the payload associated with the given `key` if available.
absl::optional<std::string> GetPayload(Status const& s,
                                       std::string const& key) {
  auto it = s.payload_.find(key);
  if (it == s.payload_.end()) return absl::nullopt;
  return it->second;
}

}  // namespace internal

std::string StatusCodeToString(StatusCode code) {
  switch (code) {
    case StatusCode::kOk:
      return "OK";
    case StatusCode::kCancelled:
      return "CANCELLED";
    case StatusCode::kUnknown:
      return "UNKNOWN";
    case StatusCode::kInvalidArgument:
      return "INVALID_ARGUMENT";
    case StatusCode::kDeadlineExceeded:
      return "DEADLINE_EXCEEDED";
    case StatusCode::kNotFound:
      return "NOT_FOUND";
    case StatusCode::kAlreadyExists:
      return "ALREADY_EXISTS";
    case StatusCode::kPermissionDenied:
      return "PERMISSION_DENIED";
    case StatusCode::kUnauthenticated:
      return "UNAUTHENTICATED";
    case StatusCode::kResourceExhausted:
      return "RESOURCE_EXHAUSTED";
    case StatusCode::kFailedPrecondition:
      return "FAILED_PRECONDITION";
    case StatusCode::kAborted:
      return "ABORTED";
    case StatusCode::kOutOfRange:
      return "OUT_OF_RANGE";
    case StatusCode::kUnimplemented:
      return "UNIMPLEMENTED";
    case StatusCode::kInternal:
      return "INTERNAL";
    case StatusCode::kUnavailable:
      return "UNAVAILABLE";
    case StatusCode::kDataLoss:
      return "DATA_LOSS";
    default:
      return "UNEXPECTED_STATUS_CODE=" + std::to_string(static_cast<int>(code));
  }
}

std::ostream& operator<<(std::ostream& os, StatusCode code) {
  return os << StatusCodeToString(code);
}

RuntimeStatusError::RuntimeStatusError(Status status)
    : std::runtime_error(StatusWhat(status)), status_(std::move(status)) {}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
