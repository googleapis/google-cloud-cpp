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

#include "google/cloud/internal/operation_id.h"
#include "google/cloud/internal/random.h"
#include "absl/strings/ascii.h"
#include <mutex>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

std::string OperationId(std::string const& prefix) {
  std::string id;
  std::size_t constexpr kOperationIdLength = 128;
  id.reserve(kOperationIdLength);

  id.append(prefix, 0, 32);
  absl::AsciiStrToLower(&id);
  id.append(1, '_');

  static auto* rng = new auto(google::cloud::internal::MakeDefaultPRNG());
  static std::mutex rng_mu;
  std::string suffix;
  {
    std::unique_lock<std::mutex> rng_lk(rng_mu);
    suffix = google::cloud::internal::Sample(
        *rng, static_cast<int>(kOperationIdLength - id.size()),
        "abcdefghijklmnopqrstuvwxyz0123456789");
  }
  id.append(suffix);

  return id;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
