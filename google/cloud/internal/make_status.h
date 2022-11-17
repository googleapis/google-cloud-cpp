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

#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

Status CancelledError(std::string msg, ErrorInfo info);
Status UnknownError(std::string msg, ErrorInfo info);
Status InvalidArgumentError(std::string msg, ErrorInfo info);
Status DeadlineExceededError(std::string msg, ErrorInfo info);
Status NotFoundError(std::string msg, ErrorInfo info);
Status AlreadyExistsError(std::string msg, ErrorInfo info);
Status PermissionDeniedError(std::string msg, ErrorInfo info);
Status UnauthenticatedError(std::string msg, ErrorInfo info);
Status ResourceExhaustedError(std::string msg, ErrorInfo info);
Status FailedPreconditionError(std::string msg, ErrorInfo info);
Status AbortedError(std::string msg, ErrorInfo info);
Status OutOfRangeError(std::string msg, ErrorInfo info);
Status UnimplementedError(std::string msg, ErrorInfo info);
Status InternalError(std::string msg, ErrorInfo info);
Status UnavailableError(std::string msg, ErrorInfo info);
Status DataLossError(std::string msg, ErrorInfo info);

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_MAKE_STATUS_H
