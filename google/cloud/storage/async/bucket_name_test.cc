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

#include "google/cloud/storage/async/bucket_name.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <sstream>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;

TEST(BucketName, Basics) {
  BucketName b("b1");
  EXPECT_EQ("b1", b.name());
  EXPECT_EQ("projects/_/buckets/b1", b.FullName());

  auto copy = b;
  EXPECT_EQ(copy, b);
  EXPECT_EQ("b1", copy.name());
  EXPECT_EQ("projects/_/buckets/b1", copy.FullName());

  auto moved = std::move(copy);
  EXPECT_EQ(moved, b);
  EXPECT_EQ("b1", moved.name());
  EXPECT_EQ("projects/_/buckets/b1", moved.FullName());

  BucketName b2("b2");
  EXPECT_NE(b2, b);
  EXPECT_EQ("b2", b2.name());
  EXPECT_EQ("projects/_/buckets/b2", b2.FullName());
}

TEST(BucketName, OutputStream) {
  BucketName b("b1");
  std::ostringstream os;
  os << b;
  EXPECT_EQ("projects/_/buckets/b1", os.str());
}

TEST(BucketName, MakeBucketName) {
  auto b = BucketName("b1");
  EXPECT_THAT(MakeBucketName(b.FullName()), IsOkAndHolds(b));

  for (std::string invalid : {
           "",
           "projects/",
           "projects/_/bucket/",
           "bad/_/buckets/b1",
           "projects/_/bad/b1",
           "projects//_/b1",
           "/projects/_/buckets/b1",
       }) {
    auto b = MakeBucketName(invalid);
    EXPECT_THAT(b, StatusIs(StatusCode::kInvalidArgument, HasSubstr(invalid)));
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google
