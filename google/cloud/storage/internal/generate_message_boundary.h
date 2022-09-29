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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GENERATE_MESSAGE_BOUNDARY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GENERATE_MESSAGE_BOUNDARY_H

#include "google/cloud/storage/version.h"
#include "google/cloud/internal/invoke_result.h"
#include "absl/functional/function_ref.h"
#include <string>
#include <type_traits>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * Generate a string that is not found in @p message.
 *
 * When sending messages over multipart MIME payloads we need a separator that
 * is not found in the body of the message *and* that is not too large (it is
 * trivial to generate a string not found in @p message, just append some
 * characters to the message itself).
 *
 * The algorithm is:
 * - First, try using fixed strings consisting of 64 copies of any valid
 *   boundary character. We can verify if one of those would work by searching
 *   for the boundary character every 64 bytes in the message. If the character
 *   never appears in that search, then there is no substring in the message
 *   consisting of 64 copies of the character.
 * - If that fails, generate a short random string, and search for it in the
 *   message. If the message has that string, append some more random characters
 *   and keep searching.
 *
 * @param message a message body, typically the payload of a HTTP request that
 *     will need to be encoded as a MIME multipart message.
 * @param random_string_generator a callable to generate random strings.
 *     Typically a lambda that captures the generator and any locks necessary
 *     to generate the string. This is also used in testing to verify things
 *     work when the initial string is found.
 * @param initial_size the length for the initial string.
 * @param growth_size how fast to grow the random string.
 * @return a string not found in @p message.
 */
std::string GenerateMessageBoundaryImpl(
    std::string const& message,
    absl::FunctionRef<std::string(int)> random_string_generator,
    int initial_size, int growth_size);

/**
 * A backwards compatible version of `GenerateMessageBoundary()`.
 *
 * The original implementation of this function predates `absl::FunctionRef` (or
 * at least its availability in `google-cloud-cpp`).  We used a template to
 * support any callable that met the requirements.
 */
template <typename RandomStringGenerator,
          typename std::enable_if<google::cloud::internal::is_invocable<
                                      RandomStringGenerator, int>::value,
                                  int>::type = 0>
std::string GenerateMessageBoundary(
    std::string const& message, RandomStringGenerator&& random_string_generator,
    int initial_size, int growth_size) {
  return GenerateMessageBoundaryImpl(
      message, std::forward<RandomStringGenerator>(random_string_generator),
      initial_size, growth_size);
}

/// Implements the slow case for `GenerateMessageBoundaryImpl()`.
std::string GenerateMessageBoundaryImplSlow(
    std::string const& message,
    absl::FunctionRef<std::string(int)> random_string_generator,
    int initial_size, int growth_size);

/// Optimize the common case in `GenerateMessageBoundaryImpl()`.
std::string MaybeGenerateMessageBoundaryImplQuick(std::string const& message);

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GENERATE_MESSAGE_BOUNDARY_H
