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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_THROW_DELEGATE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_THROW_DELEGATE_H

#include "google/cloud/status.h"
#include <system_error>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

//@{
/**
 * @name Delegate exception raising to out of line functions.
 *
 * The following functions raise the corresponding exception, unless the user
 * has disabled exception handling, in which case they call the function set by
 * `google::cloud::SetTerminateHandler()`. The default handler prints the error
 * message to `std::cerr` and calls `std::abort()`.
 *
 * We copied this technique from Abseil.  Unfortunately we cannot use it
 * directly because it is not a public interface for Abseil.
 */
[[noreturn]] void ThrowInvalidArgument(char const* msg);
[[noreturn]] void ThrowInvalidArgument(std::string const& msg);

[[noreturn]] void ThrowRangeError(char const* msg);
[[noreturn]] void ThrowRangeError(std::string const& msg);

[[noreturn]] void ThrowRuntimeError(char const* msg);
[[noreturn]] void ThrowRuntimeError(std::string const& msg);

[[noreturn]] void ThrowSystemError(std::error_code ec, char const* msg);
[[noreturn]] void ThrowSystemError(std::error_code ec, std::string const& msg);

[[noreturn]] void ThrowLogicError(char const* msg);
[[noreturn]] void ThrowLogicError(std::string const& msg);
//@}

/// Throw an exception wrapping @p status.
[[noreturn]] void ThrowStatus(Status status);

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_THROW_DELEGATE_H
