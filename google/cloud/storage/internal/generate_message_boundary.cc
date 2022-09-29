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

#include "google/cloud/storage/internal/generate_message_boundary.h"
#include <algorithm>
#include <array>
#include <limits>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

std::string GenerateMessageBoundaryImplSlow(
    std::string const& message,
    absl::FunctionRef<std::string(int)> random_string_generator,
    int initial_size, int growth_size) {
  std::string candidate = random_string_generator(initial_size);
  for (std::string::size_type i = message.find(candidate, 0);
       i != std::string::npos; i = message.find(candidate, i)) {
    candidate += random_string_generator(growth_size);
  }
  return candidate;
}

std::string MaybeGenerateMessageBoundaryImplQuick(std::string const& message) {
  static_assert((std::numeric_limits<unsigned char>::max)() == 255,
                "Review this code, it may not work on this platform.");

  static char const kBoundaryChars[] =
      "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  auto constexpr kSeenSize =
      static_cast<std::size_t>((std::numeric_limits<unsigned char>::max)()) + 1;
  auto constexpr kSkip = std::size_t{64};
  std::bitset<bool, kSeenSize> seen;
  std::fill(seen.begin(), seen.end(), false);
  for (auto i = kSkip - 1; i < message.size(); i += kSkip) {
    seen[static_cast<unsigned char>(message[i])] = true;
  }
  for (auto c : kBoundaryChars) {
    if (c == '\0') continue;
    if (!seen[static_cast<unsigned char>(c)]) return std::string(kSkip, c);
  }
  return std::string{};
}

std::string GenerateMessageBoundaryImpl(
    std::string const& message,
    absl::FunctionRef<std::string(int)> random_string_generator,
    int initial_size, int growth_size) {
  auto candidate = MaybeGenerateMessageBoundaryImplQuick(message);
  if (!candidate.empty()) return candidate;

  return GenerateMessageBoundaryImplSlow(
      message, std::move(random_string_generator), initial_size, growth_size);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
