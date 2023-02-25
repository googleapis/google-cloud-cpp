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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TERMINATE_HANDLER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TERMINATE_HANDLER_H

#include "google/cloud/version.h"
#include <functional>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
/**
 * @defgroup terminate Intercepting Unrecoverable Errors
 *
 * @brief Helper types and functions to report unrecoverable errors.
 *
 * In some rare cases, the client libraries may need to terminate the
 * application because it encounters an unrecoverable error. For example:
 *
 * - If the application calls `StatusOr<T>::value()`, and the library was
 *   compiled with exceptions disabled, and the `StatusOr<T>` contains an error,
 *   *then* the function throws an exception to report the error as the function
 *   cannot return a valid value.  Applications that disable exceptions
 *   should query the `StatusOr<T>` status (using `.ok()` or `.status()`) and
 *   avoid calling `.value()` if the `StatusOr<T>` is holding an error.
 * - If the application calls `future<T>::get()`, the library was compiled with
 *   exceptions disabled, and (somehow) the future is satisfied with an
 *   exception. Note that the library APIs typically return
 *   `future<StatusOr<T>>` to avoid this problem, but the application may
 *   have created `future<T>` and `promise<T>` pairs in their own code.
 *
 * In these cases there is no mechanism to return the error. The library cannot
 * continue working correctly and must terminate the program. The application
 * may want to intercept these errors, before the application crashes, and log
 * or otherwise capture additional information to help with debugging or
 * troubleshooting. The functions in this module can be used to do so.
 *
 * By their nature, there is no mechanism to "handle" and "recover" from
 * unrecoverable errors. All the application can do is log additional
 * information before the program terminates.
 *
 * Note that the libraries do not use functions that can trigger unrecoverable
 * errors (if they do we consider that a library bug).
 *
 * The default behavior in the client library is to call `std::abort()` when
 * an unrecoverable error occurs.
 */

/**
 * Terminate handler.
 *
 * It should handle the error, whose description are given in *msg* and should
 * never return.
 *
 * @ingroup terminate
 */
using TerminateHandler = std::function<void(char const* msg)>;

/**
 * Install terminate handler and get the old one atomically.
 *
 * @param f the handler. It should never return, behaviour is undefined
 *        otherwise.
 *
 * @return Previously set handler.
 *
 * @ingroup terminate
 */
TerminateHandler SetTerminateHandler(TerminateHandler f);

/**
 * Get the currently installed handler.
 *
 * @return The currently installed handler.
 *
 * @ingroup terminate
 */
TerminateHandler GetTerminateHandler();

/**
 * Invoke the currently installed handler.
 *
 * @param msg Details about the error.
 *
 * This function should never return.
 *
 * @ingroup terminate
 */
[[noreturn]] void Terminate(char const* msg);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TERMINATE_HANDLER_H
