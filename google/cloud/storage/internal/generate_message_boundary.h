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
#include "google/cloud/internal/random.h"
#include "absl/functional/function_ref.h"
#include <string>

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
 * The algorithm is to generate a short random string, and search for it in the
 * message, if the message has that string, generate a new string and retry.
 * The strings are 64 (alphanumeric) characters long, the number of permutations
 * is large enough that a suitable string will be found eventually.
 *
 * @param message a message body, typically the payload of a HTTP request that
 *     will need to be encoded as a MIME multipart message.
 * @param candidate_generator a callable to generate random strings.
 *     Typically a lambda that captures the generator and any locks necessary
 *     to generate the string. This is also used in testing to verify things
 *     work when the initial string is found.
 * @return a string not found in @p message.
 */
std::string GenerateMessageBoundary(
    std::string const& message,
    absl::FunctionRef<std::string()> candidate_generator);

/// A helper to generate message boundary candidates.
std::string GenerateMessageBoundaryCandidate(
    google::cloud::internal::DefaultPRNG& generator);

/**
 * @deprecated use another `GenerateMessageBoundary()` overload.
 */
template <typename RandomStringGenerator,
          typename std::enable_if<google::cloud::internal::is_invocable<
                                      RandomStringGenerator, int>::value,
                                  int>::type = 0>
GOOGLE_CLOUD_CPP_DEPRECATED(
    "deprecated, using the GenerateMessageBoundary(std::string const&, "
    "absl::FunctionRef<std::string()>) overload")
std::string
    GenerateMessageBoundary(std::string const& message,
                            RandomStringGenerator&& random_string_generator,
                            int /*initial_size*/, int /*growth_size*/) {
  return GenerateMessageBoundary(message, [&random_string_generator]() {
    return random_string_generator(64);
  });
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GENERATE_MESSAGE_BOUNDARY_H
