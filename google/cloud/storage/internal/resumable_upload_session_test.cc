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

#include "google/cloud/storage/internal/resumable_upload_session.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::testing::HasSubstr;

TEST(ResumableUploadResponseTest, Base) {
  auto actual = ResumableUploadResponse::FromHttpResponse(
                    HttpResponse{200,
                                 R"""({"name": "test-object-name"})""",
                                 {{"ignored-header", "value"},
                                  {"location", "location-value"},
                                  {"range", "bytes=0-2000"}}})
                    .value();
  ASSERT_TRUE(actual.payload.has_value());
  EXPECT_EQ("test-object-name", actual.payload->name());
  EXPECT_EQ("location-value", actual.upload_session_url);
  EXPECT_EQ(2000, actual.last_committed_byte);
  EXPECT_EQ(ResumableUploadResponse::kDone, actual.upload_state);

  std::ostringstream os;
  os << actual;
  auto actual_str = os.str();
  EXPECT_THAT(actual_str, HasSubstr("upload_session_url=location-value"));
  EXPECT_THAT(actual_str, HasSubstr("last_committed_byte=2000"));
}

TEST(ResumableUploadResponseTest, NoLocation) {
  auto actual = ResumableUploadResponse::FromHttpResponse(
                    HttpResponse{308, {}, {{"range", "bytes=0-2000"}}})
                    .value();
  EXPECT_FALSE(actual.payload.has_value());
  EXPECT_EQ("", actual.upload_session_url);
  EXPECT_EQ(2000, actual.last_committed_byte);
  EXPECT_EQ(ResumableUploadResponse::kInProgress, actual.upload_state);
}

TEST(ResumableUploadResponseTest, NoRange) {
  auto actual = ResumableUploadResponse::FromHttpResponse(
                    HttpResponse{201,
                                 R"""({"name": "test-object-name"})""",
                                 {{"location", "location-value"}}})
                    .value();
  ASSERT_TRUE(actual.payload.has_value());
  EXPECT_EQ("test-object-name", actual.payload->name());
  EXPECT_EQ("location-value", actual.upload_session_url);
  EXPECT_EQ(0, actual.last_committed_byte);
  EXPECT_EQ(ResumableUploadResponse::kDone, actual.upload_state);
}

TEST(ResumableUploadResponseTest, MissingBytesInRange) {
  auto actual = ResumableUploadResponse::FromHttpResponse(
                    HttpResponse{308,
                                 {},
                                 {{"location", "location-value"},
                                  {"range", "units=0-2000"}}})
                    .value();
  EXPECT_FALSE(actual.payload.has_value());
  EXPECT_EQ("location-value", actual.upload_session_url);
  EXPECT_EQ(0, actual.last_committed_byte);
  EXPECT_EQ(ResumableUploadResponse::kInProgress, actual.upload_state);
}

TEST(ResumableUploadResponseTest, MissingRangeEnd) {
  auto actual = ResumableUploadResponse::FromHttpResponse(
                    HttpResponse{308, {}, {{"range", "bytes=0-"}}})
                    .value();
  EXPECT_FALSE(actual.payload.has_value());
  EXPECT_EQ("", actual.upload_session_url);
  EXPECT_EQ(0, actual.last_committed_byte);
  EXPECT_EQ(ResumableUploadResponse::kInProgress, actual.upload_state);
}

TEST(ResumableUploadResponseTest, InvalidRangeEnd) {
  auto actual = ResumableUploadResponse::FromHttpResponse(
                    HttpResponse{308, {}, {{"range", "bytes=0-abcd"}}})
                    .value();
  EXPECT_FALSE(actual.payload.has_value());
  EXPECT_EQ("", actual.upload_session_url);
  EXPECT_EQ(0, actual.last_committed_byte);
  EXPECT_EQ(ResumableUploadResponse::kInProgress, actual.upload_state);
}

TEST(ResumableUploadResponseTest, InvalidRangeBegin) {
  auto actual = ResumableUploadResponse::FromHttpResponse(
                    HttpResponse{308, {}, {{"range", "bytes=abcd-2000"}}})
                    .value();
  EXPECT_FALSE(actual.payload.has_value());
  EXPECT_EQ("", actual.upload_session_url);
  EXPECT_EQ(0, actual.last_committed_byte);
  EXPECT_EQ(ResumableUploadResponse::kInProgress, actual.upload_state);
}

TEST(ResumableUploadResponseTest, UnexpectedRangeBegin) {
  auto actual = ResumableUploadResponse::FromHttpResponse(
                    HttpResponse{308, {}, {{"range", "bytes=3000-2000"}}})
                    .value();
  EXPECT_FALSE(actual.payload.has_value());
  EXPECT_EQ("", actual.upload_session_url);
  EXPECT_EQ(0, actual.last_committed_byte);
  EXPECT_EQ(ResumableUploadResponse::kInProgress, actual.upload_state);
}

TEST(ResumableUploadResponseTest, NegativeEnd) {
  auto actual = ResumableUploadResponse::FromHttpResponse(
                    HttpResponse{308, {}, {{"range", "bytes=0--7"}}})
                    .value();
  EXPECT_FALSE(actual.payload.has_value());
  EXPECT_EQ("", actual.upload_session_url);
  EXPECT_EQ(0, actual.last_committed_byte);
  EXPECT_EQ(ResumableUploadResponse::kInProgress, actual.upload_state);
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
