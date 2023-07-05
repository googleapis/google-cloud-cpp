// Copyright 2021 Google LLC
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

#include "google/cloud/internal/retry_policy_impl.h"
#include "absl/strings/match.h"
#include <algorithm>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

bool IsTransientInternalError(Status const& status) {
  // Treat the unexpected termination of the gRPC connection as retryable.
  // There is no explicit indication of this, but it will result in an
  // INTERNAL status with one of the `kTransientFailureMessages`.
  static constexpr char const* kTransientFailureMessages[] = {
      "RST_STREAM", "Received Rst Stream",
      "Received unexpected EOS on DATA frame from server"};

  if (status.code() != StatusCode::kInternal) return false;
  return std::any_of(std::begin(kTransientFailureMessages),
                     std::end(kTransientFailureMessages),
                     [&status](char const* message) {
                       return absl::StrContains(status.message(), message);
                     });
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
