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

#include "google/cloud/storage/object_stream.h"
#include "google/cloud/storage/internal/raw_client.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::NotNull;

TEST(ObjectStream, ReadMoveConstructor) {
  ObjectReadStream reader;
  ASSERT_THAT(reader.rdbuf(), NotNull());
  reader.setstate(std::ios::badbit | std::ios::eofbit);
  EXPECT_TRUE(reader.bad());
  EXPECT_TRUE(reader.eof());

  ObjectReadStream copy(std::move(reader));
  ASSERT_THAT(copy.rdbuf(), NotNull());
  EXPECT_TRUE(copy.bad());
  EXPECT_TRUE(copy.eof());
  EXPECT_THAT(copy.status(), StatusIs(StatusCode::kUnimplemented));

  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_THAT(reader.status(), Not(IsOk()));
  EXPECT_THAT(reader.rdbuf(), NotNull());
}

TEST(ObjectStream, ReadMoveAssignment) {
  ObjectReadStream reader;
  ASSERT_THAT(reader.rdbuf(), NotNull());
  reader.setstate(std::ios::badbit | std::ios::eofbit);

  ObjectReadStream copy;

  copy = std::move(reader);
  ASSERT_THAT(copy.rdbuf(), NotNull());
  EXPECT_TRUE(copy.bad());
  EXPECT_TRUE(copy.eof());
  EXPECT_THAT(copy.status(), StatusIs(StatusCode::kUnimplemented));

  // NOLINTNEXTLINE(bugprone-use-after-move)
  ASSERT_THAT(reader.rdbuf(), NotNull());
  EXPECT_THAT(reader.status(), Not(IsOk()));
}

TEST(ObjectStream, WriteMoveConstructor) {
  ObjectWriteStream writer;
  ASSERT_THAT(writer.rdbuf(), NotNull());
  EXPECT_THAT(writer.metadata(), StatusIs(StatusCode::kUnimplemented));

  ObjectWriteStream copy(std::move(writer));
  ASSERT_THAT(copy.rdbuf(), NotNull());
  EXPECT_TRUE(copy.bad());
  EXPECT_TRUE(copy.eof());
  EXPECT_THAT(copy.metadata(), StatusIs(StatusCode::kUnimplemented));

  // NOLINTNEXTLINE(bugprone-use-after-move)
  ASSERT_THAT(writer.rdbuf(), NotNull());
  EXPECT_THAT(writer.last_status(), Not(IsOk()));
}

TEST(ObjectStream, WriteMoveAssignment) {
  ObjectWriteStream writer;
  ASSERT_THAT(writer.rdbuf(), NotNull());
  EXPECT_THAT(writer.metadata(), StatusIs(StatusCode::kUnimplemented));

  ObjectWriteStream copy;

  copy = std::move(writer);
  ASSERT_THAT(copy.rdbuf(), NotNull());
  EXPECT_TRUE(copy.bad());
  EXPECT_TRUE(copy.eof());
  EXPECT_THAT(copy.metadata(), StatusIs(StatusCode::kUnimplemented));

  // NOLINTNEXTLINE(bugprone-use-after-move)
  ASSERT_THAT(writer.rdbuf(), NotNull());
  EXPECT_THAT(writer.last_status(), Not(IsOk()));
}

TEST(ObjectStream, Suspend) {
  ObjectWriteStream writer;
  ASSERT_THAT(writer.rdbuf(), NotNull());
  EXPECT_THAT(writer.metadata(), StatusIs(StatusCode::kUnimplemented));

  std::move(writer).Suspend();
  // NOLINTNEXTLINE(bugprone-use-after-move)
  ASSERT_THAT(writer.rdbuf(), NotNull());
  EXPECT_THAT(writer.last_status(), Not(IsOk()));
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
