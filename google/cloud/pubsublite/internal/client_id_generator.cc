// Copyright 2023 Google LLC
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

#include "google/cloud/pubsublite/internal/client_id_generator.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/version.h"
#include <algorithm>
#include <iterator>

namespace google {
namespace cloud {
namespace pubsublite_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::string GenerateClientId() {
  auto constexpr kIdLength = 16;

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  std::uniform_int_distribution<int> random_bytes(
      std::numeric_limits<char>::min(), std::numeric_limits<char>::max());

  std::string client_id;
  client_id.reserve(kIdLength);
  std::generate_n(std::back_inserter(client_id), kIdLength,
                  [&] { return static_cast<char>(random_bytes(generator)); });
  return client_id;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google
