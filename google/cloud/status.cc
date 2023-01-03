// Copyright 2018 Google LLC
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

bool operator==(ErrorInfo const& a, ErrorInfo const& b) {
  return a.reason_ == b.reason_ && a.domain_ == b.domain_ &&
         a.metadata_ == b.metadata_;
}

bool operator!=(ErrorInfo const& a, ErrorInfo const& b) { return !(a == b); }

// Encapsulates the implementation of a non-OK status. OK Statuses are
// represented by a nullptr Status::impl_, as an optimization for the common
// case of OK Statuses. This class holds all the data associated with a non-OK
// Status so we don't have to worry about bloating the common OK case.
class Status::Impl {
 public:
  using PayloadType = std::unordered_map<std::string, std::string>;

  explicit Impl(StatusCode code, std::string message, ErrorInfo info,
                PayloadType payload)
      : code_(code),
        message_(std::move(message)),
        error_info_(std::move(info)),
        payload_(std::move(payload)) {}

  StatusCode code() const { return code_; }
  std::string const& message() const { return message_; }
  ErrorInfo const& error_info() const { return error_info_; }
  PayloadType const& payload() const { return payload_; };

  // Allows mutable access to payload, which is needed in the
  // `internal::SetPayload()` function.
  PayloadType& payload() { return payload_; };

  friend inline bool operator==(Impl const& a, Impl const& b) {
    return a.code_ == b.code_ && a.message_ == b.message_ &&
           a.error_info_ == b.error_info_ && a.payload_ == b.payload_;
  }

  friend inline bool operator!=(Impl const& a, Impl const& b) {
    return !(a == b);
  }

 private:
  StatusCode code_;
  std::string message_;
  ErrorInfo error_info_;
  PayloadType payload_;
};

Status::Status() = default;
Status::~Status() = default;
Status::Status(Status&&) noexcept = default;
Status& Status::operator=(Status&&) noexcept = default;

// Deep copy
Status::Status(Status const& other)
    : impl_(other.ok() ? nullptr : new auto(*other.impl_)) {}

// Deep copy
Status& Status::operator=(Status const& other) {
  impl_.reset(other.ok() ? nullptr : new auto(*other.impl_));
  return *this;
}

// OK statuses have an impl_ == nullptr. Non-OK Statuses get an Impl.
Status::Status(StatusCode code, std::string message, ErrorInfo info)
    : impl_(code == StatusCode::kOk
                ? nullptr
                : new Status::Impl{
                      code, std::move(message), std::move(info), {}}) {}

StatusCode Status::code() const {
  return impl_ ? impl_->code() : StatusCode::kOk;
}

std::string const& Status::message() const {
  static auto const* const kEmpty = new std::string{};
  return impl_ ? impl_->message() : *kEmpty;
}

ErrorInfo const& Status::error_info() const {
  static auto const* const kEmpty = new ErrorInfo{};
  return impl_ ? impl_->error_info() : *kEmpty;
}

bool Status::Equals(Status const& a, Status const& b) {
  return (a.ok() && b.ok()) || (a.impl_ && b.impl_ && *a.impl_ == *b.impl_);
}

std::ostream& operator<<(std::ostream& os, Status const& s) {
  if (s.ok()) return os << StatusCode::kOk;
  os << s.code() << ": " << s.message();
  auto const& e = s.error_info();
  if (e.reason().empty() && e.domain().empty() && e.metadata().empty()) {
    return os;
  }
  os << " error_info={reason=" << e.reason();
  os << ", domain=" << e.domain();
  os << ", metadata={";
  char const* sep = "";
  for (auto const& e : e.metadata()) {
    os << sep << e.first << "=" << e.second;
    sep = ", ";
  }
  return os << "}}";
}

namespace internal {

// Sets the given `payload`, indexed by the given `key`, on the given `Status`,
// IFF the status is not OK. Payloads are considered in equality comparisons.
// The keyspace used here is separate from other keyspaces (e.g.,
// `absl::Status`), so we only need to coordinate keys with ourselves.
void SetPayload(Status& s, std::string key, std::string payload) {
  if (s.impl_) s.impl_->payload()[std::move(key)] = std::move(payload);
}

// Returns the payload associated with the given `key`, if available.
absl::optional<std::string> GetPayload(Status const& s,
                                       std::string const& key) {
  if (!s.impl_) return absl::nullopt;
  auto const& payload = s.impl_->payload();
  auto it = payload.find(key);
  if (it == payload.end()) return absl::nullopt;
  return it->second;
}

}  // namespace internal

RuntimeStatusError::RuntimeStatusError(Status status)
    : std::runtime_error(StatusWhat(status)), status_(std::move(status)) {}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
