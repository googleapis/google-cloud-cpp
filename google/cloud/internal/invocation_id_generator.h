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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_INVOCATION_ID_GENERATOR_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_INVOCATION_ID_GENERATOR_H

#include "google/cloud/internal/random.h"
#include "google/cloud/version.h"
#include <mutex>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * Generate invocation ids (aka request ids).
 *
 * Some services accept a request id field (or header) to determine if a request
 * is a retry attempt. Such services return the previous result of the request,
 * effectively making the request retry idempotent.
 */
class InvocationIdGenerator {
 public:
  InvocationIdGenerator();

  /**
   * Creates a new invocation ID.
   *
   * This function is thread safe.
   */
  std::string MakeInvocationId();

 private:
  std::mutex mu_;
  google::cloud::internal::DefaultPRNG generator_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_INVOCATION_ID_GENERATOR_H
