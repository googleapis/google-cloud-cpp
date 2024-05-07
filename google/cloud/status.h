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

#include "google/cloud/internal/retry_info.h"
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

  /// `kCancelled` (gRPC code `CANCELLED`) indicates the operation was
  /// cancelled, typically by the caller.
  kCancelled = 1,

  /// `kUnknown` (gRPC code `UNKNOWN`) indicates an unknown error occurred.
  ///
  /// In general, more specific errors should be raised, if possible. Errors
  /// raised by APIs that do not return enough error information may be
  /// converted to this error.
  kUnknown = 2,

  /// `kInvalidArgument` (gRPC code `INVALID_ARGUMENT`) indicates the caller
  /// specified an invalid argument, such as a malformed filename.
  ///
  /// Note that use of such errors should be narrowly limited to indicate the
  /// invalid nature of the arguments themselves. Errors with validly formed
  /// arguments that may cause errors with the state of the receiving system
  /// should be denoted with `kFailedPrecondition` instead.
  kInvalidArgument = 3,

  /// `kDeadlineExceeded` (gRPC code `DEADLINE_EXCEEDED`) indicates a deadline
  /// expired before the operation could complete.
  ///
  /// For operations that may change state within a system, this error may be
  /// returned even if the operation has completed successfully. For example, a
  /// successful response from a server could have been delayed long enough for
  /// the deadline to expire.
  kDeadlineExceeded = 4,

  /// `kNotFound` (gRPC code `NOT_FOUND`) indicates some requested entity (such
  /// as a file or directory) was not found.
  ///
  /// `kNotFound` is useful if a request should be denied for an entire class of
  /// users, such as during a gradual feature rollout or undocumented allow
  /// list.
  /// If a request should be denied for specific sets of users, such as through
  /// user-based access control, use `kPermissionDenied` instead.
  kNotFound = 5,

  /// `kAlreadyExists` (gRPC code `ALREADY_EXISTS`) indicates that the entity a
  /// caller attempted to create (such as a file or directory) is already
  /// present.
  kAlreadyExists = 6,

  /// `kPermissionDenied` (gRPC code `PERMISSION_DENIED`) indicates that the
  /// caller does not have permission to execute the specified operation.
  ///
  /// Note that this error is different than an error due to an
  /// *un*authenticated caller. This error code does not imply the request is
  /// valid or the requested entity exists or satisfies any other
  /// pre-conditions.
  ///
  /// `kPermissionDenied` must not be used for rejections caused by exhausting
  /// some resource. Instead, use `kResourceExhausted` for those errors.
  /// `kPermissionDenied` must not be used if the caller cannot be identified.
  /// Instead, use `kUnauthenticated` for those errors.
  kPermissionDenied = 7,

  /// `kResourceExhausted` (gRPC code `RESOURCE_EXHAUSTED`) indicates some
  /// resource has been exhausted.
  ///
  /// Examples include a per-user quota, or the entire file system being out of
  /// space.
  kResourceExhausted = 8,

  /// `kFailedPrecondition` (gRPC code `FAILED_PRECONDITION`) indicates that the
  /// operation was rejected because the system is not in a state required for
  /// the operation's execution.
  ///
  /// For example, a directory to be deleted may be non-empty, a "rmdir"
  /// operation is applied to a non-directory, etc.
  ///
  /// Some guidelines that may help a service implementer in deciding between
  /// `kFailedPrecondition`, `kAborted`, and `kUnavailable`:
  ///
  /// 1. Use `kUnavailable` if the client can retry just the failing call.
  /// 2. Use `kAborted` if the client should retry at a higher transaction
  ///    level (such as when a client-specified test-and-set fails, indicating
  ///    the client should restart a read-modify-write sequence).
  /// 3. Use `kFailedPrecondition` if the client should not retry until the
  ///    system state has been explicitly fixed. For example, if a "rmdir" fails
  ///    because the directory is non-empty, `kFailedPrecondition` should be
  ///    returned since the client should not retry unless the files are deleted
  ///    from the directory.
  kFailedPrecondition = 9,

  /// `kAborted` (gRPC code `ABORTED`) indicates the operation was aborted.
  ///
  /// This is typically due to a concurrency issue such as a sequencer check
  /// failure or a failed transaction.
  ///
  /// See the guidelines above for deciding between `kFailedPrecondition`,
  /// `kAborted`, and `kUnavailable`.
  kAborted = 10,

  /// `kOutOfRange` (gRPC code `OUT_OF_RANGE`) indicates the operation was
  /// attempted past the valid range, such as seeking or reading past an
  /// end-of-file.
  ///
  /// Unlike `kInvalidArgument`, this error indicates a problem that may
  /// be fixed if the system state changes. For example, a 32-bit file
  /// system will generate `kInvalidArgument` if asked to read at an
  /// offset that is not in the range [0,2^32-1], but it will generate
  /// `kOutOfRange` if asked to read from an offset past the current
  /// file size.
  ///
  /// There is a fair bit of overlap between `kFailedPrecondition` and
  /// `kOutOfRange`.  We recommend using `kOutOfRange` (the more specific
  /// error) when it applies so that callers who are iterating through
  /// a space can easily look for an `kOutOfRange` error to detect when
  /// they are done.
  kOutOfRange = 11,

  /// `kUnimplemented` (gRPC code `UNIMPLEMENTED`) indicates the operation is
  /// not implemented or supported in this service.
  ///
  /// In this case, the operation should not be re-attempted.
  kUnimplemented = 12,

  /// `kInternal` (gRPC code `INTERNAL`) indicates an internal error has
  /// occurred and some invariants expected by the underlying system have not
  /// been satisfied.
  ///
  /// While this error code is reserved for serious errors, some services return
  /// this error under overload conditions.
  kInternal = 13,

  /// `kUnavailable` (gRPC code `UNAVAILABLE`) indicates the service is
  /// currently unavailable and that this is most likely a transient condition.
  ///
  /// An error such as this can be corrected by retrying with a backoff scheme.
  /// Note that it is not always safe to retry non-idempotent operations.
  ///
  /// See the guidelines above for deciding between `kFailedPrecondition`,
  /// `kAborted`, and `kUnavailable`.
  kUnavailable = 14,

  /// `kDataLoss` (gRPC code `DATA_LOSS`) indicates that unrecoverable data loss
  /// or corruption has occurred.
  ///
  /// As this error is serious, proper alerting should be attached to errors
  /// such as this.
  kDataLoss = 15,

  /// `kUnauthenticated` (gRPC code `UNAUTHENTICATED`) indicates that the
  /// request does not have valid authentication credentials for the operation.
  ///
  /// Correct the authentication and try again.
  kUnauthenticated = 16,
};

/// Convert @p code to a human readable string.
std::string StatusCodeToString(StatusCode code);

/// Integration with `std::iostreams`.
std::ostream& operator<<(std::ostream& os, StatusCode code);

class Status;
class ErrorInfo;
namespace internal {
void AddMetadata(ErrorInfo&, std::string const& key, std::string value);
void SetRetryInfo(Status& status, absl::optional<RetryInfo> retry_info);
absl::optional<RetryInfo> GetRetryInfo(Status const& status);
void SetPayload(Status&, std::string key, std::string payload);
absl::optional<std::string> GetPayload(Status const&, std::string const& key);
}  // namespace internal

/**
 * Describes the cause of the error with structured details.
 *
 * @see https://cloud.google.com/apis/design/errors#error_info
 */
class ErrorInfo {
 public:
  /**
   * Default constructor.
   *
   * Post-condition: the `reason()`, `domain()`, and `metadata()` fields are
   * empty.
   */
  ErrorInfo() = default;

  /**
   * Constructor.
   *
   * @param reason initializes the `reason()` value.
   * @param domain initializes the `domain()` value.
   * @param metadata initializes the `metadata()` value.
   */
  explicit ErrorInfo(std::string reason, std::string domain,
                     std::unordered_map<std::string, std::string> metadata)
      : reason_(std::move(reason)),
        domain_(std::move(domain)),
        metadata_(std::move(metadata)) {}

  /**
   * The reason of the error.
   *
   * This is a constant value that identifies the proximate cause of the error.
   * Error reasons are unique within a particular domain of errors. This should
   * be at most 63 characters and match a regular expression of
   * `[A-Z][A-Z0-9_]+[A-Z0-9]`, which represents UPPER_SNAKE_CASE.
   */
  std::string const& reason() const { return reason_; }

  /**
   * The logical grouping to which the "reason" belongs.
   *
   * The error domain is typically the registered service name of the tool or
   * product that generates the error. Example: "pubsub.googleapis.com". If the
   * error is generated by some common infrastructure, the error domain must be
   * a globally unique value that identifies the infrastructure. For Google API
   * infrastructure, the error domain is "googleapis.com".
   *
   * For errors generated by the C++ client libraries the domain is
   * `gcloud-cpp`.
   */
  std::string const& domain() const { return domain_; }

  /**
   * Additional structured details about this error.
   *
   * Keys should match the regular expression `[a-zA-Z0-9-_]` and be limited
   * to 64 characters in length.
   *
   * When identifying the current value of an exceeded limit, the units should
   * be contained in the key, not the value.  For example, if the client exceeds
   * the number of instances that can be created in a single (batch) request
   * return `{"instanceLimitPerRequest": "100"}` rather than
   * `{"instanceLimit": "100/request"}`.
   */
  std::unordered_map<std::string, std::string> const& metadata() const {
    return metadata_;
  }

  friend bool operator==(ErrorInfo const&, ErrorInfo const&);
  friend bool operator!=(ErrorInfo const&, ErrorInfo const&);

 private:
  friend void internal::AddMetadata(ErrorInfo&, std::string const& key,
                                    std::string value);

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
  /// Default constructor, initializes to `StatusCode::kOk`.
  Status();
  /// Destructor.
  ~Status();
  ///@{
  /**
   * @name Copy construction and assignment.
   */
  Status(Status const&);
  Status& operator=(Status const&);
  ///@}
  ///@{
  /**
   * @name Move construction and assignment.
   */
  Status(Status&&) noexcept;
  Status& operator=(Status&&) noexcept;
  ///@}

  /**
   * Construct from a status code, message and (optional) error info.
   *
   * @param code the status code for the new `Status`.
   * @param message the message for the new `Status`, ignored if @p code is
   *     `StatusCode::kOk`.
   * @param info the `ErrorInfo` for the new `Status`, ignored if @p code is
   *     `StatusCode::kOk`.
   */
  explicit Status(StatusCode code, std::string message, ErrorInfo info = {});

  /// Returns true if the status code is `StatusCode::kOk`.
  bool ok() const { return !impl_; }

  /// Returns the status code.
  StatusCode code() const;

  /**
   * Returns the message associated with the status.
   *
   * This is always empty if `code()` is `StatusCode::kOk`.
   */
  std::string const& message() const;

  /**
   * Returns the additional error info associated with the status.
   *
   * This is always a default-constructed error info if `code()` is
   * `StatusCode::kOk`.
   */
  ErrorInfo const& error_info() const;

  friend bool operator==(Status const& a, Status const& b) {
    return (a.ok() && b.ok()) || Equals(a, b);
  }
  friend bool operator!=(Status const& a, Status const& b) { return !(a == b); }

 private:
  static bool Equals(Status const& a, Status const& b);
  friend void internal::SetRetryInfo(Status&,
                                     absl::optional<internal::RetryInfo>);
  friend absl::optional<internal::RetryInfo> internal::GetRetryInfo(
      Status const&);
  friend void internal::SetPayload(Status&, std::string, std::string);
  friend absl::optional<std::string> internal::GetPayload(Status const&,
                                                          std::string const&);

  class Impl;
  // A null `impl_` is an OK status. Only non-OK Statuses allocate an Impl.
  std::unique_ptr<Impl> impl_;
};

/**
 * Stream @p s to @p os.
 *
 * This in intended for logging and troubleshooting. Applications should not
 * depend on the format of this output.
 */
std::ostream& operator<<(std::ostream& os, Status const& s);

/**
 * A runtime error that wraps a `google::cloud::Status`.
 */
class RuntimeStatusError : public std::runtime_error {
 public:
  /// Constructor from a `Status`.
  explicit RuntimeStatusError(Status status);

  /// Returns the original status.
  Status const& status() const { return status_; }

 private:
  Status status_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STATUS_H
