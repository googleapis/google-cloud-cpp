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

#include "google/cloud/storage/internal/generate_message_boundary.h"
#include <algorithm>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

std::string GenerateMessageBoundary(
    std::string const& message,
    absl::FunctionRef<std::string()> candidate_generator) {
  auto candidate = candidate_generator();
  while (message.find(candidate) != std::string::npos) {
    candidate = candidate_generator();
  }
  return candidate;
}

std::string GenerateMessageBoundaryCandidate(
    google::cloud::internal::DefaultPRNG& generator) {
  auto constexpr kSize = 64;
  auto candidate = std::string{
      "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"};
  std::shuffle(candidate.begin(), candidate.end(), generator);
  return candidate.substr(0, kSize);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
