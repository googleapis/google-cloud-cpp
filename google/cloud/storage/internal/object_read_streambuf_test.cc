// Copyright 2021 Google LLC
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

#include "google/cloud/storage/internal/object_read_streambuf.h"
#include "google/cloud/storage/testing/mock_client.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::testing::Return;

TEST(ObjectReadStreambufTest, FailedTellg) {
  ObjectReadStreambuf buf(ReadObjectRangeRequest{},
                          Status(StatusCode::kInvalidArgument, "some error"));
  std::istream stream(&buf);
  EXPECT_EQ(-1, stream.tellg());
}

TEST(ObjectReadStreambufTest, Success) {
  auto read_source = absl::make_unique<testing::MockObjectReadSource>();
  EXPECT_CALL(*read_source, IsOpen()).WillRepeatedly(Return(true));
  EXPECT_CALL(*read_source, Read)
      .WillOnce(Return(ReadSourceResult{10, {}}))
      .WillOnce(Return(ReadSourceResult{15, {}}))
      .WillOnce(Return(ReadSourceResult{15, {}}))
      .WillOnce(Return(ReadSourceResult{128 * 1024, {}}));
  ObjectReadStreambuf buf(ReadObjectRangeRequest{}, std::move(read_source), 0);

  std::istream stream(&buf);
  EXPECT_EQ(0, stream.tellg());

  auto read = [&](std::size_t to_read, std::streampos expected_tellg) {
    std::vector<char> v(to_read);
    stream.read(v.data(), v.size());
    EXPECT_TRUE(!!stream);
    EXPECT_EQ(expected_tellg, stream.tellg());
  };
  read(10, 10);
  read(15, 25);
  read(15, 40);
  stream.get();
  EXPECT_EQ(41, stream.tellg());
  read(1000, 1041);
  read(2000, 3041);
  // Reach eof
  std::vector<char> v(128 * 1024 - 1 - 1000 - 2000);
  stream.read(v.data(), v.size());
  EXPECT_EQ(128 * 1024 + 15 + 15 + 10, stream.tellg());
}

TEST(ObjectReadStreambufTest, WrongSeek) {
  auto read_source = absl::make_unique<testing::MockObjectReadSource>();
  EXPECT_CALL(*read_source, IsOpen()).WillRepeatedly(Return(true));
  EXPECT_CALL(*read_source, Read).WillOnce(Return(ReadSourceResult{10, {}}));
  ObjectReadStreambuf buf(ReadObjectRangeRequest{}, std::move(read_source), 0);

  std::istream stream(&buf);
  EXPECT_EQ(0, stream.tellg());

  std::vector<char> v(10);
  stream.read(v.data(), v.size());
  EXPECT_TRUE(!!stream);
  EXPECT_EQ(10, stream.tellg());
  EXPECT_FALSE(stream.fail());
  stream.seekg(10);
  EXPECT_TRUE(stream.fail());
  stream.clear();
  stream.seekg(-1, std::ios_base::cur);
  EXPECT_TRUE(stream.fail());
  stream.clear();
  stream.seekg(0, std::ios_base::beg);
  EXPECT_TRUE(stream.fail());
  stream.clear();
  stream.seekg(0, std::ios_base::end);
  EXPECT_TRUE(stream.fail());
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
