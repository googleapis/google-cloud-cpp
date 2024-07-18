// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/async/reader_connection_factory.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include <google/protobuf/text_format.h>
#include <google/storage/v2/storage.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsProtoEqual;
using ::google::protobuf::TextFormat;

TEST(AsyncReaderConnectionFactory, UpdateGenerationDefault) {
  google::storage::v2::ReadObjectRequest expected;
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        bucket: "projects/_/buckets/test-bucket" object: "test-object"
      )pb",
      &expected));
  auto actual = expected;
  UpdateGeneration(actual, storage::Generation());
  EXPECT_THAT(actual, IsProtoEqual(expected));

  UpdateGeneration(actual, storage::Generation(1234));
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        bucket: "projects/_/buckets/test-bucket"
        object: "test-object"
        generation: 1234
      )pb",
      &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));

  UpdateGeneration(actual, storage::Generation(1234));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(AsyncReaderConnectionFactory, UpdateGenerationWithGeneration) {
  google::storage::v2::ReadObjectRequest expected;
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        bucket: "projects/_/buckets/test-bucket"
        object: "test-object"
        generation: 1234
      )pb",
      &expected));
  auto actual = expected;
  UpdateGeneration(actual, storage::Generation());
  EXPECT_THAT(actual, IsProtoEqual(expected));

  UpdateGeneration(actual, storage::Generation(2345));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(AsyncReaderConnectionFactory, UpdateReadRangeDefault) {
  google::storage::v2::ReadObjectRequest expected;
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        bucket: "projects/_/buckets/test-bucket" object: "test-object"
      )pb",
      &expected));
  auto actual = expected;
  UpdateReadRange(actual, 1000);
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        bucket: "projects/_/buckets/test-bucket"
        object: "test-object"
        read_offset: 1000
      )pb",
      &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
  UpdateReadRange(actual, 500);
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        bucket: "projects/_/buckets/test-bucket"
        object: "test-object"
        read_offset: 1500
      )pb",
      &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(AsyncReaderConnectionFactory, UpdateReadRangeWithRange) {
  google::storage::v2::ReadObjectRequest expected;
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        bucket: "projects/_/buckets/test-bucket"
        object: "test-object"
        read_offset: 1000
        read_limit: 1000000
      )pb",
      &expected));
  auto actual = expected;
  UpdateReadRange(actual, 1000);
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        bucket: "projects/_/buckets/test-bucket"
        object: "test-object"
        read_offset: 2000
        read_limit: 999000
      )pb",
      &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
  UpdateReadRange(actual, 500);
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        bucket: "projects/_/buckets/test-bucket"
        object: "test-object"
        read_offset: 2500
        read_limit: 998500
      )pb",
      &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(AsyncReaderConnectionFactory, UpdateReadRangeFromOffset) {
  google::storage::v2::ReadObjectRequest expected;
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        bucket: "projects/_/buckets/test-bucket"
        object: "test-object"
        read_offset: 1000000
      )pb",
      &expected));
  auto actual = expected;
  UpdateReadRange(actual, 1000);
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        bucket: "projects/_/buckets/test-bucket"
        object: "test-object"
        read_offset: 1001000
      )pb",
      &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
  UpdateReadRange(actual, 500);
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        bucket: "projects/_/buckets/test-bucket"
        object: "test-object"
        read_offset: 1001500
      )pb",
      &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(AsyncReaderConnectionFactory, UpdateReadRangeLast) {
  google::storage::v2::ReadObjectRequest expected;
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        bucket: "projects/_/buckets/test-bucket"
        object: "test-object"
        read_offset: -1000000
      )pb",
      &expected));
  auto actual = expected;
  UpdateReadRange(actual, 1000);
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        bucket: "projects/_/buckets/test-bucket"
        object: "test-object"
        read_offset: -999000
      )pb",
      &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
  UpdateReadRange(actual, 500);
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        bucket: "projects/_/buckets/test-bucket"
        object: "test-object"
        read_offset: -998500
      )pb",
      &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(AsyncReaderConnectionFactory, UpdateReadRangeUnexpected) {
  google::storage::v2::ReadObjectRequest expected;
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        bucket: "projects/_/buckets/test-bucket"
        object: "test-object"
        read_offset: 1000
        read_limit: 1000
      )pb",
      &expected));
  auto actual = expected;
  UpdateReadRange(actual, -1000);
  EXPECT_THAT(actual, IsProtoEqual(expected));

  UpdateReadRange(actual, 2000);
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        bucket: "projects/_/buckets/test-bucket"
        object: "test-object"
        read_offset: 1000
        read_limit: -1
      )pb",
      &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
