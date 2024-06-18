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

#include "google/cloud/storage/bucket_object_retention.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include <gmock/gmock.h>
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

TEST(BucketObjectRetention, Compare) {
  auto const a = BucketObjectRetention{true};
  auto const b = BucketObjectRetention{false};

  EXPECT_EQ(a, a);
  EXPECT_NE(a, b);
}

TEST(BucketObjectRetention, IOStream) {
  std::ostringstream os;
  os << BucketObjectRetention{true};
  auto const actual = os.str();
  EXPECT_EQ(actual, "BucketObjectRetention={enabled=true}");
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
