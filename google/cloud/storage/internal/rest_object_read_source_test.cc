// Copyright 2022 Google LLC
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

#include "google/cloud/storage/internal/rest_object_read_source.h"
#include "google/cloud/testing_util/mock_http_payload.h"
#include "google/cloud/testing_util/mock_rest_response.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::rest_internal::HttpStatusCode;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::MockHttpPayload;
using ::google::cloud::testing_util::MockRestResponse;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Return;

TEST(RestObjectReadSourceTest, IsOpen) {
  auto mock_response = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response, StatusCode).WillOnce(Return(HttpStatusCode::kOk));
  EXPECT_CALL(*mock_response, Headers)
      .WillOnce(Return(std::multimap<std::string, std::string>()));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload).WillOnce([&] {
    auto mock_payload = absl::make_unique<MockHttpPayload>();
    EXPECT_CALL(*mock_payload, HasUnreadData).WillOnce([]() { return true; });
    return mock_payload;
  });

  RestObjectReadSource read_source(std::move(mock_response));
  EXPECT_TRUE(read_source.IsOpen());
  (void)read_source.Close();
  EXPECT_FALSE(read_source.IsOpen());
}

TEST(RestObjectReadSourceTest, Close) {
  auto mock_response = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response, StatusCode).WillOnce(Return(HttpStatusCode::kOk));
  EXPECT_CALL(*mock_response, Headers)
      .WillOnce(Return(std::multimap<std::string, std::string>()));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload).WillOnce([&] {
    auto mock_payload = absl::make_unique<MockHttpPayload>();
    return mock_payload;
  });

  RestObjectReadSource read_source(std::move(mock_response));
  auto result = read_source.Close();
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(result->status_code, Eq(HttpStatusCode::kOk));
  EXPECT_THAT(result->headers, IsEmpty());

  result = read_source.Close();
  EXPECT_THAT(result, StatusIs(StatusCode::kFailedPrecondition));
}

TEST(RestObjectReadSourceTest, ReadAfterClose) {
  auto mock_response = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response, StatusCode).WillOnce(Return(HttpStatusCode::kOk));
  EXPECT_CALL(*mock_response, Headers)
      .WillOnce(Return(std::multimap<std::string, std::string>()));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload).WillOnce([&] {
    auto mock_payload = absl::make_unique<MockHttpPayload>();
    return mock_payload;
  });

  RestObjectReadSource read_source(std::move(mock_response));
  auto result = read_source.Close();
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(result->status_code, Eq(HttpStatusCode::kOk));
  EXPECT_THAT(result->headers, IsEmpty());

  std::array<char, 2048> buf;
  auto read_result = read_source.Read(buf.data(), buf.size());
  EXPECT_THAT(read_result, StatusIs(StatusCode::kFailedPrecondition));
}

TEST(RestObjectReadSourceTest, ReadNotFound) {
  auto mock_response = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response, StatusCode)
      .WillOnce(Return(HttpStatusCode::kNotFound));
  EXPECT_CALL(*mock_response, Headers)
      .WillOnce(Return(std::multimap<std::string, std::string>()));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload).WillOnce([&] {
    auto mock_payload = absl::make_unique<MockHttpPayload>();
    return mock_payload;
  });

  RestObjectReadSource read_source(std::move(mock_response));
  std::array<char, 2048> buf;
  auto result = read_source.Read(buf.data(), buf.size());
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(result->bytes_received, Eq(0));
  EXPECT_THAT(result->response.status_code, Eq(HttpStatusCode::kNotFound));
}

TEST(RestObjectReadSourceTest, ReadAllDataDecompressiveTranscoding) {
  std::string const payload("A man, a plan, Panama!");
  auto mock_response = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response, StatusCode).WillOnce(Return(HttpStatusCode::kOk));
  EXPECT_CALL(*mock_response, Headers)
      .WillOnce(Return(std::multimap<std::string, std::string>(
          {{"x-guploader-response-body-transformations", "gunzipped"}})));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload).WillOnce([&] {
    auto mock_payload = absl::make_unique<MockHttpPayload>();
    {
      testing::InSequence s;
      EXPECT_CALL(*mock_payload, Read).WillOnce([&](absl::Span<char> buffer) {
        std::copy(payload.data(), payload.data() + payload.size(),
                  buffer.data());
        return payload.size();
      });
      EXPECT_CALL(*mock_payload, HasUnreadData).WillOnce(Return(false));
    }
    return mock_payload;
  });

  RestObjectReadSource read_source(std::move(mock_response));
  std::array<char, 2048> buf;
  auto result = read_source.Read(buf.data(), buf.size());
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(result->response.status_code, Eq(HttpStatusCode::kOk));
  EXPECT_THAT(result->transformation, Eq("gunzipped"));
  EXPECT_THAT(result->bytes_received, Eq(payload.size()));
  EXPECT_THAT(std::string(buf.data(), result->bytes_received), Eq(payload));
}

TEST(RestObjectReadSourceTest, ReadSomeData) {
  std::string const payload("A man, a plan, Panama!");
  auto mock_response = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response, StatusCode).WillOnce(Return(HttpStatusCode::kOk));
  EXPECT_CALL(*mock_response, Headers)
      .WillOnce(Return(std::multimap<std::string, std::string>()));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload).WillOnce([&] {
    auto mock_payload = absl::make_unique<MockHttpPayload>();
    {
      testing::InSequence s;
      EXPECT_CALL(*mock_payload, Read).WillOnce([&](absl::Span<char> buffer) {
        std::copy(payload.data(), payload.data() + payload.size(),
                  buffer.data());
        return payload.size();
      });
      EXPECT_CALL(*mock_payload, HasUnreadData).WillOnce(Return(true));
    }
    return mock_payload;
  });

  RestObjectReadSource read_source(std::move(mock_response));
  std::array<char, 2048> buf;
  auto result = read_source.Read(buf.data(), buf.size());
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(result->response.status_code, Eq(HttpStatusCode::kContinue));
  EXPECT_THAT(result->bytes_received, Eq(payload.size()));
  EXPECT_THAT(std::string(buf.data(), result->bytes_received), Eq(payload));
}

struct HeaderHashTest {
  std::string name;
  std::multimap<std::string, std::string> headers;
  HashValues expected_hashes;
  absl::optional<std::int64_t> expected_generation;
};

class RestObjectReadSourceHeadersTest
    : public ::testing::TestWithParam<HeaderHashTest> {};

TEST_P(RestObjectReadSourceHeadersTest, ReadResultHeaders) {
  std::string const payload("A man, a plan, Panama!");
  auto mock_response = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response, StatusCode).WillOnce(Return(HttpStatusCode::kOk));
  EXPECT_CALL(*mock_response, Headers)
      .WillOnce(
          Return(std::multimap<std::string, std::string>(GetParam().headers)));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload).WillOnce([&] {
    auto mock_payload = absl::make_unique<MockHttpPayload>();
    {
      testing::InSequence s;
      EXPECT_CALL(*mock_payload, Read).WillOnce([&](absl::Span<char> buffer) {
        std::copy(payload.data(), payload.data() + payload.size(),
                  buffer.data());
        return payload.size();
      });
      EXPECT_CALL(*mock_payload, HasUnreadData).WillOnce(Return(false));
    }
    return mock_payload;
  });

  RestObjectReadSource read_source(std::move(mock_response));
  std::array<char, 2048> buf;
  auto result = read_source.Read(buf.data(), buf.size());
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(result->response.status_code, Eq(HttpStatusCode::kOk));
  EXPECT_THAT(result->transformation, Eq(absl::nullopt));
  EXPECT_THAT(result->bytes_received, Eq(payload.size()));
  EXPECT_THAT(std::string(buf.data(), result->bytes_received), Eq(payload));
  auto param = GetParam();
  EXPECT_EQ(param.expected_generation, result->generation);
  EXPECT_EQ(param.expected_hashes.crc32c, result->hashes.crc32c);
  EXPECT_EQ(param.expected_hashes.md5, result->hashes.md5);
}

INSTANTIATE_TEST_SUITE_P(
    ReadResultHeaderProcessing, RestObjectReadSourceHeadersTest,
    testing::Values(HeaderHashTest{"empty", {}, {}, absl::nullopt},
                    HeaderHashTest{"irrelevant_headers",
                                   {{"x-generation", "123"},
                                    {"x-goog-stuff", "thing"},
                                    {"x-hashes", "crc32c=123"}},
                                   {},
                                   absl::nullopt},
                    HeaderHashTest{
                        "generation", {{"x-goog-generation", "123"}}, {}, 123},
                    HeaderHashTest{"hashes",
                                   {{"x-goog-hash", "md5=123, crc32c=abc"}},
                                   {"abc", "123"},
                                   absl::nullopt},
                    HeaderHashTest{"split_hashes",
                                   {{"x-goog-hash", "md5=123"},
                                    {"x-goog-hash", "crc32c=abc"}},
                                   {"abc", "123"},
                                   absl::nullopt},
                    HeaderHashTest{"hashes_and_generation",
                                   {{"x-goog-hash", "md5=123, crc32c=abc"},
                                    {"x-goog-generation", "456"}},
                                   {"abc", "123"},
                                   456}),
    [](testing::TestParamInfo<RestObjectReadSourceHeadersTest::ParamType> const&
           info) { return info.param.name; });

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
