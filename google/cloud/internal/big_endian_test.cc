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

#include "google/cloud/internal/big_endian.h"
#include <gmock/gmock.h>
#include <cstring>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

TEST(BigEndianTest, Int16) {
  std::uint8_t buf[] = {0x01, 0x02};
  std::int16_t value;
  static_assert(sizeof(value) == sizeof(buf), "Mismatched sizes");
  std::memcpy(&value, buf, sizeof(buf));
  EXPECT_EQ(0x0102, FromBigEndian(value));
  EXPECT_EQ(value, FromBigEndian(ToBigEndian(value)));
}

TEST(BigEndianTest, UInt16) {
  std::uint8_t buf[] = {0x01, 0x02};
  std::uint16_t value;
  static_assert(sizeof(value) == sizeof(buf), "Mismatched sizes");
  std::memcpy(&value, buf, sizeof(buf));
  EXPECT_EQ(0x0102U, FromBigEndian(value));
  EXPECT_EQ(value, FromBigEndian(ToBigEndian(value)));
}

TEST(BigEndianTest, Int32) {
  std::uint8_t buf[] = {0x01, 0x02, 0x03, 0x04};
  std::int32_t value;
  static_assert(sizeof(value) == sizeof(buf), "Mismatched sizes");
  std::memcpy(&value, buf, sizeof(buf));
  EXPECT_EQ(0x01020304, FromBigEndian(value));
  EXPECT_EQ(value, FromBigEndian(ToBigEndian(value)));
}

TEST(BigEndianTest, UInt32) {
  std::uint8_t buf[] = {0x01, 0x02, 0x03, 0x04};
  std::uint32_t value;
  static_assert(sizeof(value) == sizeof(buf), "Mismatched sizes");
  std::memcpy(&value, buf, sizeof(buf));
  EXPECT_EQ(0x01020304U, FromBigEndian(value));
  EXPECT_EQ(value, FromBigEndian(ToBigEndian(value)));
}

TEST(BigEndianTest, Int64) {
  std::uint8_t buf[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  std::int64_t value;
  static_assert(sizeof(value) == sizeof(buf), "Mismatched sizes");
  std::memcpy(&value, buf, sizeof(buf));
  EXPECT_EQ(0x0102030405060708, FromBigEndian(value));
  EXPECT_EQ(value, FromBigEndian(ToBigEndian(value)));
}

TEST(BigEndianTest, UInt64) {
  std::uint8_t buf[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  std::uint64_t value;
  static_assert(sizeof(value) == sizeof(buf), "Mismatched sizes");
  std::memcpy(&value, buf, sizeof(buf));
  EXPECT_EQ(0x0102030405060708U, FromBigEndian(value));
  EXPECT_EQ(value, FromBigEndian(ToBigEndian(value)));
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
