// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/binary_data_as_debug_string.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

TEST(BinaryDataAsDebugStringTest, Simple) {
  auto actual = BinaryDataAsDebugString("123abc", 6);
  EXPECT_EQ(
      "123abc                   "
      "313233616263                                    \n",
      actual);
}

TEST(BinaryDataAsDebugStringTest, Multiline) {
  auto actual =
      BinaryDataAsDebugString(" 123456789 123456789 123456789 123456789", 40);
  EXPECT_EQ(
      " 123456789 123456789 123 "
      "203132333435363738392031323334353637383920313233\n"
      "456789 123456789         "
      "34353637383920313233343536373839                \n",
      actual);
}

TEST(BinaryDataAsDebugStringTest, Blanks) {
  auto actual = BinaryDataAsDebugString("\n \r \t \v \b \a \f ", 14);
  EXPECT_EQ(
      ". . . . . . .            "
      "0a200d2009200b20082007200c20                    \n",
      actual);
}

TEST(BinaryDataAsDebugStringTest, NonPrintable) {
  auto actual = BinaryDataAsDebugString("\x03\xf1 abcd", 7);
  EXPECT_EQ(
      ".. abcd                  "
      "03f12061626364                                  \n",
      actual);
}

TEST(BinaryDataAsDebugStringTest, Limit) {
  auto actual = BinaryDataAsDebugString(
      " 123456789 123456789 123456789 123456789", 40, 24);
  EXPECT_EQ(
      " 123456789 123456789 123 "
      "203132333435363738392031323334353637383920313233\n",
      actual);
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
