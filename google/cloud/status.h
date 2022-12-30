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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STATUS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STATUS_H

#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Well-known status codes with `grpc::StatusCode`-compatible values.
 *
 * The semantics of these values are documented in:
 *     https://grpc.io/grpc/cpp/classgrpc_1_1_status.html
 */
enum class StatusCode {
  /// Not an error; returned on success.
  kOk = 0,

  kCancelled = 1,
  kUnknown = 2,
  kInvalidArgument = 3,
  kDeadlineExceeded = 4,
  kNotFound = 5,
  kAlreadyExists = 6,
  kPermissionDenied = 7,
  kUnauthenticated = 16,
  kResourceExhausted = 8,
  kFailedPrecondition = 9,
  kAborted = 10,
  kOutOfRange = 11,
  kUnimplemented = 12,
  kInternal = 13,
  kUnavailable = 14,
  kDataLoss = 15,
};

std::string StatusCodeToString(StatusCode code);
std::ostream& operator<<(std::ostream& os, StatusCode code);

class Status;
namespace internal {
void SetPayload(Status&, std::string key, std::string payload);
absl::optional<std::string> GetPayload(Status const&, std::string const& key);
Status WithStatusCode(Status, StatusCode code);
}  // namespace internal

/**
 * Describes the cause of the error with structured details.
 *
 * @see https://cloud.google.com/apis/design/errors#error_info
 */
class ErrorInfo {
 public:
  ErrorInfo() = default;
  explicit ErrorInfo(std::string reason, std::string domain,
                     std::unordered_map<std::string, std::string> metadata)
      : reason_(std::move(reason)),
        domain_(std::move(domain)),
        metadata_(std::move(metadata)) {}

  std::string const& reason() const { return reason_; }
  std::string const& domain() const { return domain_; }
  std::unordered_map<std::string, std::string> const& metadata() const {
    return metadata_;
  }

  friend bool operator==(ErrorInfo const&, ErrorInfo const&);
  friend bool operator!=(ErrorInfo const&, ErrorInfo const&);

 private:
  std::string reason_;
  std::string domain_;
  std::unordered_map<std::string, std::string> metadata_;
};

/**
 * Represents success or an error with info about the error.
 *
 * This class is typically used to indicate whether or not a function or other
 * operation completed successfully. Success is indicated by an "OK" status. OK
 * statuses will have `.code() == StatusCode::kOk` and `.ok() == true`, with
 * all other properties having empty values. All OK statuses are equal. Any
 * non-OK `Status` is considered an error. Users can inspect the error using
 * the member functions, or they can simply stream the `Status` object, and it
 * will print itself in some human readable way (the streamed format may change
 * over time and you should *not* depend on the specific format of a streamed
 * `Status` object remaining unchanged).
 *
 * This is a regular value type that can be copied, moved, compared for
 * equality, and streamed.
 */
class Status {
 public:
  Status();
  ~Status();
  Status(Status const&);
  Status& operator=(Status const&);
  Status(Status&&) noexcept;
  Status& operator=(Status&&) noexcept;

  /**
   * Constructs a Status with the given @p code and @p message.
   *
   * Ignores @p message if @p code is `StatusCode::kOk`.
   */
  explicit Status(StatusCode code, std::string message, ErrorInfo info = {});

  bool ok() const { return !impl_; }
  StatusCode code() const;
  std::string const& message() const;
  ErrorInfo const& error_info() const;

  friend inline bool operator==(Status const& a, Status const& b) {
    return (a.ok() && b.ok()) || Equals(a, b);
  }
  friend inline bool operator!=(Status const& a, Status const& b) {
    return !(a == b);
  }
  friend std::ostream& operator<<(std::ostream& os, Status const& s);

 private:
  class Impl;
  explicit Status(std::unique_ptr<Impl> impl);
  static bool Equals(Status const& a, Status const& b);
  friend void internal::SetPayload(Status&, std::string, std::string);
  friend absl::optional<std::string> internal::GetPayload(Status const&,
                                                          std::string const&);
  friend Status internal::WithStatusCode(Status, StatusCode code);

  // A null `impl_` is an OK status. Only non-OK Statuses allocate an Impl.
  std::unique_ptr<Impl> impl_;
};

/**
 * A runtime error that wraps a `google::cloud::Status`.
 */
class RuntimeStatusError : public std::runtime_error {
 public:
  explicit RuntimeStatusError(Status status);

  Status const& status() const { return status_; }

 private:
  Status status_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STATUS_H
