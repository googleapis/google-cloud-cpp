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
#include "google/cloud/internal/make_unique.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

ObjectReadStream CreateReader() {
  using google::cloud::internal::make_unique;
  std::shared_ptr<internal::RawClient> client;
  internal::ReadObjectRangeRequest request("test-bucket", "test-object");

  auto buf = make_unique<internal::ObjectReadStreambuf>(
      request, Status(StatusCode::kNotFound, "test-message"));
  return ObjectReadStream(std::move(buf));
}

ObjectWriteStream CreateWriter() {
  auto session = google::cloud::internal::make_unique<
      internal::ResumableUploadSessionError>(
      Status(StatusCode::kNotFound, "test-message"));

  auto validator =
      google::cloud::internal::make_unique<internal::NullHashValidator>();

  ObjectWriteStream writer(
      google::cloud::internal::make_unique<internal::ObjectWriteStreambuf>(
          std::move(session), 0, std::move(validator)));
  writer.setstate(std::ios::badbit | std::ios::eofbit);
  writer.Close();
  return writer;
}

TEST(ObjectStream, ReadMoveConstructor) {
  ObjectReadStream reader = CreateReader();
  reader.setstate(std::ios::badbit | std::ios::eofbit);
  EXPECT_TRUE(reader.bad());
  EXPECT_TRUE(reader.eof());
  EXPECT_NE(nullptr, reader.rdbuf());

  ObjectReadStream copy(std::move(reader));
  EXPECT_TRUE(copy.bad());
  EXPECT_TRUE(copy.eof());
  EXPECT_EQ(StatusCode::kNotFound, copy.status().code());

  EXPECT_EQ(nullptr, reader.rdbuf());
  EXPECT_NE(nullptr, copy.rdbuf());
}

TEST(ObjectStream, ReadMoveAssignment) {
  ObjectReadStream reader = CreateReader();
  reader.setstate(std::ios::badbit | std::ios::eofbit);
  EXPECT_NE(nullptr, reader.rdbuf());

  ObjectReadStream copy;

  copy = std::move(reader);
  EXPECT_TRUE(copy.bad());
  EXPECT_TRUE(copy.eof());
  EXPECT_EQ(StatusCode::kNotFound, copy.status().code());

  EXPECT_EQ(nullptr, reader.rdbuf());
  EXPECT_NE(nullptr, copy.rdbuf());
}

TEST(ObjectStream, WriteMoveConstructor) {
  ObjectWriteStream writer = CreateWriter();
  EXPECT_EQ(StatusCode::kNotFound, writer.metadata().status().code());
  EXPECT_NE(nullptr, writer.rdbuf());

  ObjectWriteStream copy(std::move(writer));
  EXPECT_TRUE(copy.bad());
  EXPECT_TRUE(copy.eof());
  EXPECT_EQ(StatusCode::kNotFound, copy.metadata().status().code());

  EXPECT_EQ(nullptr, writer.rdbuf());
  EXPECT_NE(nullptr, copy.rdbuf());
}

TEST(ObjectStream, WriteMoveAssignment) {
  ObjectWriteStream writer = CreateWriter();
  EXPECT_EQ(StatusCode::kNotFound, writer.metadata().status().code());
  EXPECT_NE(nullptr, writer.rdbuf());

  ObjectWriteStream copy;

  copy = std::move(writer);
  EXPECT_TRUE(copy.bad());
  EXPECT_TRUE(copy.eof());
  EXPECT_EQ(StatusCode::kNotFound, copy.metadata().status().code());

  EXPECT_EQ(nullptr, writer.rdbuf());
  EXPECT_NE(nullptr, copy.rdbuf());
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
