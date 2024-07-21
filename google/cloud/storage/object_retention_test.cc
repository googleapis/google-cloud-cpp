// Copyright 2024 Google LLC
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

#include "google/cloud/storage/object_retention.h"
#include "google/cloud/internal/format_time_point.h"
#include <gmock/gmock.h>
#include <sstream>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::HasSubstr;

TEST(ObjectRetention, OStream) {
  auto const ts = std::chrono::system_clock::now() + std::chrono::hours(24);
  auto const input = ObjectRetention{"test-only-invalid", ts};

  std::ostringstream os;
  os << input;
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr("ObjectRetention={"));
  EXPECT_THAT(actual, HasSubstr("mode=test-only-invalid"));
  EXPECT_THAT(actual, HasSubstr("retain_until_time=" +
                                google::cloud::internal::FormatRfc3339(ts)));
}

TEST(ObjectRetention, Compare) {
  auto const ts_a = std::chrono::system_clock::now() + std::chrono::hours(24);
  auto const ts_b = std::chrono::system_clock::now() + std::chrono::hours(48);

  auto const a = ObjectRetention{"a", ts_a};
  auto const b = ObjectRetention{"b", ts_b};
  auto const c = ObjectRetention{"a", ts_b};
  auto const d = ObjectRetention{"b", ts_a};

  EXPECT_EQ(a, a);
  EXPECT_EQ(b, b);
  EXPECT_NE(a, b);
  EXPECT_NE(a, c);
  EXPECT_NE(a, d);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
