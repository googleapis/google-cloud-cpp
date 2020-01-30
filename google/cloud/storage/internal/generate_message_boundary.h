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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GENERATE_MESSAGE_BOUNDARY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GENERATE_MESSAGE_BOUNDARY_H

#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/storage/version.h"
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
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
 * message, if the message has that string, append some more random characters
 * and keep searching.
 *
 * This function is a template because the string generator is typically a
 * lambda that captures state variables (such as the random number generator),
 * of the class that uses it.
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
template <typename RandomStringGenerator,
          typename std::enable_if<google::cloud::internal::is_invocable<
                                      RandomStringGenerator, int>::value,
                                  int>::type = 0>
std::string GenerateMessageBoundary(
    std::string const& message, RandomStringGenerator&& random_string_generator,
    int initial_size, int growth_size) {
  std::string candidate = random_string_generator(initial_size);
  for (std::string::size_type i = message.find(candidate, 0);
       i != std::string::npos; i = message.find(candidate, i)) {
    candidate += random_string_generator(growth_size);
  }
  return candidate;
}
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GENERATE_MESSAGE_BOUNDARY_H
