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

#include "google/cloud/storage/bucket_soft_delete_policy.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "absl/time/time.h"
#include <gmock/gmock.h>
#include <sstream>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

TEST(BucketSoftDeletePolicy, Compare) {
  auto const now = std::chrono::system_clock::now();
  auto const a = BucketSoftDeletePolicy{std::chrono::seconds(123), now};
  auto const b = BucketSoftDeletePolicy{std::chrono::seconds(1234), now};
  auto const c = BucketSoftDeletePolicy{std::chrono::seconds(123),
                                        now + std::chrono::seconds(5)};

  EXPECT_EQ(a, a);
  EXPECT_NE(a, b);
  EXPECT_NE(a, c);
}

TEST(BucketSoftDeletePolicy, IOStream) {
  auto constexpr kTimestamp = "2022-10-10T12:34:56.789Z";

  absl::Time t;
  std::string error;
  ASSERT_TRUE(absl::ParseTime(absl::RFC3339_full, kTimestamp, &t, &error))
      << "error=" << error;
  auto tp = absl::ToChronoTime(t);

  std::ostringstream os;
  os << BucketSoftDeletePolicy{std::chrono::seconds(123), tp};
  auto const actual = os.str();
  EXPECT_EQ(
      actual,
      absl::StrCat(
          "BucketSoftDeletePolicy={retention_duration=123s, effective_time=",
          kTimestamp, "}"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
