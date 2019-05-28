// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/object_streambuf.h"
#include "google/cloud/storage/object_metadata.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

TEST(ObjectStreambufTest, WriteErrorStreambuf) {
  Status expected(StatusCode::kUnknown, "test-message");
  ObjectWriteErrorStreambuf streambuf(expected);

  EXPECT_TRUE(streambuf.IsOpen());

  auto response = streambuf.Close();
  EXPECT_EQ(expected, response.status());
  EXPECT_FALSE(streambuf.IsOpen());

  // These are mostly to increase code coverage, we want to find the important
  // missing coverage, and adding 4 lines of tests saves us guessing later.
  EXPECT_FALSE(streambuf.ValidateHash(ObjectMetadata()));
  EXPECT_EQ("", streambuf.computed_hash());
  EXPECT_EQ("", streambuf.received_hash());
  EXPECT_EQ("", streambuf.resumable_session_id());
  EXPECT_EQ(0, streambuf.next_expected_byte());

  // The error status should still be set.
  response = streambuf.Close();
  EXPECT_EQ(expected, response.status());
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
