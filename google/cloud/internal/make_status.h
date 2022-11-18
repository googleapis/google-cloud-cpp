// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_MAKE_STATUS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_MAKE_STATUS_H

#include "google/cloud/internal/error_metadata.h"
#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

Status CancelledError(std::string msg, ErrorInfo info = {});
Status UnknownError(std::string msg, ErrorInfo info = {});
Status InvalidArgumentError(std::string msg, ErrorInfo info = {});
Status DeadlineExceededError(std::string msg, ErrorInfo info = {});
Status NotFoundError(std::string msg, ErrorInfo info = {});
Status AlreadyExistsError(std::string msg, ErrorInfo info = {});
Status PermissionDeniedError(std::string msg, ErrorInfo info = {});
Status UnauthenticatedError(std::string msg, ErrorInfo info = {});
Status ResourceExhaustedError(std::string msg, ErrorInfo info = {});
Status FailedPreconditionError(std::string msg, ErrorInfo info = {});
Status AbortedError(std::string msg, ErrorInfo info = {});
Status OutOfRangeError(std::string msg, ErrorInfo info = {});
Status UnimplementedError(std::string msg, ErrorInfo info = {});
Status InternalError(std::string msg, ErrorInfo info = {});
Status UnavailableError(std::string msg, ErrorInfo info = {});
Status DataLossError(std::string msg, ErrorInfo info = {});

/**
 * Build `ErrorInfo` instances from parts.
 *
 * @par Example
 *
 * This is typically used in conjunction with the `GCP_ERROR_INFO()` macro. To
 * return an error with minimal annotations use:
 *
 * @code
 * StatusOr<double> SquareRoot(double x) {
 *   if (x < 0) return OutOfRangeError("negative input", GCP_ERROR_INFO());
 *   return std::sqrt(x);
 * }
 * @endcode
 *
 * To include more annotations you could use:
 *
 * @code
 * StatusOr<std::string> GetString(
 *     nlohmann::json const& json, absl::string_view key,
 *     ErrorContext const& ec) {
 *   auto i = json.find(key);
 *   if (i = json.end()) {
 *     return InvalidArgument(
 *         "missing key",
 *         GCP_ERROR_INFO().WithContext(ec).WithMetadata("key", key));
 *   }
 *   return i->get<std::string>(); // no type checking in this example
 * }
 * @endcode
 *
 */
class ErrorInfoBuilder {
 public:
  ErrorInfoBuilder(std::string file, int line, std::string function);

  /// Adds the metadata from an error context, existing values are not replaced.
  ErrorInfoBuilder&& WithContext(ErrorContext const& ec) && {
    metadata_.insert(ec.begin(), ec.end());
    return std::move(*this);
  }

  /// Add a metadata pair, existing values are not replaced.
  ErrorInfoBuilder&& WithMetadata(absl::string_view key,
                                  absl::string_view value) && {
    metadata_.emplace(key, value);
    return std::move(*this);
  }

  ErrorInfoBuilder&& WithReason(std::string reason) && {
    reason_ = std::move(reason);
    return std::move(*this);
  }

  ErrorInfo Build(StatusCode code) &&;

 private:
  absl::optional<std::string> reason_;
  std::unordered_map<std::string, std::string> metadata_;
};

#define GCP_ERROR_INFO() \
  ::google::cloud::internal::ErrorInfoBuilder(__FILE__, __LINE__, __func__)

Status CancelledError(std::string msg, ErrorInfoBuilder b);
Status UnknownError(std::string msg, ErrorInfoBuilder b);
Status InvalidArgumentError(std::string msg, ErrorInfoBuilder b);
Status DeadlineExceededError(std::string msg, ErrorInfoBuilder b);
Status NotFoundError(std::string msg, ErrorInfoBuilder b);
Status AlreadyExistsError(std::string msg, ErrorInfoBuilder b);
Status PermissionDeniedError(std::string msg, ErrorInfoBuilder b);
Status UnauthenticatedError(std::string msg, ErrorInfoBuilder b);
Status ResourceExhaustedError(std::string msg, ErrorInfoBuilder b);
Status FailedPreconditionError(std::string msg, ErrorInfoBuilder b);
Status AbortedError(std::string msg, ErrorInfoBuilder b);
Status OutOfRangeError(std::string msg, ErrorInfoBuilder b);
Status UnimplementedError(std::string msg, ErrorInfoBuilder b);
Status InternalError(std::string msg, ErrorInfoBuilder b);
Status UnavailableError(std::string msg, ErrorInfoBuilder b);
Status DataLossError(std::string msg, ErrorInfoBuilder b);

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_MAKE_STATUS_H
