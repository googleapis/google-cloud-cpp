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
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsublite_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::testing::SizeIs;

TEST(GenerateClientIdTest, GeneratesUniqueIds) {
  auto constexpr kNumIds = 100;
  std::set<std::string> client_ids;
  for (int i = 0; i < kNumIds; ++i) {
    auto client_id = GenerateClientId();
    EXPECT_THAT(client_id, SizeIs(16));
    client_ids.insert(client_id);
  }
  EXPECT_THAT(client_ids, SizeIs(kNumIds));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google
