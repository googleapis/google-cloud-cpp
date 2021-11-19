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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STATUS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STATUS_H

#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <iostream>
#include <string>
#include <tuple>
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
}  // namespace internal

/**
 * Represents success or an error with info about the error.

 * This class is typically used to indicate whether or not a function or other
 * operation completed successfully. Success is indicated by an "OK" status
 * (`StatusCode::kOk`), and the `.ok()` method will return true. OK statuses
 * will have an empty `.message()`, and all OK statuses are equivalent. Any
 * non-OK `Status` is considered an error. Users can inspect the error using
 * the `.code()` and `.message()` member functions, or they can simply stream
 * the `Status` object, and it will print itself in some human readable way
 * (the streamed format may change over time and you should *not* depend on the
 * specific format of a streamed `Status` object remaining unchanged).
 *
 * This is a regular value type that can be copied, moved, compared for
 * equality, and streamed.
 */
class Status {
 public:
  Status() = default;
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
  explicit Status(StatusCode code, std::string message);

  bool ok() const { return impl_ == nullptr; }
  StatusCode code() const;
  std::string const& message() const;

  friend bool operator==(Status const& a, Status const& b);
  friend bool operator!=(Status const& a, Status const& b);
  friend std::ostream& operator<<(std::ostream& os, Status const& s);

 private:
  friend void internal::SetPayload(Status&, std::string, std::string);
  friend absl::optional<std::string> internal::GetPayload(Status const&,
                                                          std::string const&);

  class Impl;
  // A null `impl_` is an OK status. Only non-OK Statuses allocate an Impl.
  Impl* impl_{nullptr};
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
