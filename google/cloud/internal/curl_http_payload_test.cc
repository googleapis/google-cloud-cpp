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

#include "google/cloud/internal/curl_http_payload.h"
#include "google/cloud/testing_util/mock_http_payload.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::MakeMockHttpPayloadSuccess;
using ::google::cloud::testing_util::MockHttpPayload;
using ::testing::Eq;

TEST(HttpPayloadTest, ReadAllEmpty) {
  auto payload = absl::make_unique<MockHttpPayload>();
  EXPECT_CALL(*payload, Read).WillOnce([](absl::Span<char> const&) {
    return 0;
  });
  auto result = ReadAll(std::move(payload));
  EXPECT_STATUS_OK(result);
  EXPECT_TRUE(result->empty());
}

TEST(HttpPayloadTest, ReadAllOneReadCall) {
  std::string response = "Hello World!";
  auto payload = MakeMockHttpPayloadSuccess(response);

  auto result = ReadAll(std::move(payload));
  EXPECT_STATUS_OK(result);
  EXPECT_THAT(*result, Eq(response));
}

TEST(HttpPayloadTest, ReadAllMultipleReadCalls) {
  std::string response = "Hello World!";
  std::size_t read_size = 5;
  auto payload = absl::make_unique<MockHttpPayload>();
  EXPECT_CALL(*payload, Read)
      .WillOnce([&](absl::Span<char> buffer) {
        std::copy(response.begin(), response.begin() + read_size,
                  buffer.begin());
        return read_size;
      })
      .WillOnce([&](absl::Span<char> buffer) {
        std::copy(response.begin() + read_size,
                  response.begin() + 2 * read_size, buffer.begin());
        return read_size;
      })
      .WillOnce([&](absl::Span<char> buffer) {
        std::copy(response.begin() + 2 * read_size, response.end(),
                  buffer.begin());
        return response.size() - 2 * read_size;
      })
      .WillOnce([](absl::Span<char> const&) { return 0; });

  auto result = ReadAll(std::move(payload), read_size);
  EXPECT_STATUS_OK(result);
  EXPECT_THAT(*result, Eq(response));
}

TEST(HttpPayloadTest, ReadAllReadError) {
  auto payload = absl::make_unique<MockHttpPayload>();
  EXPECT_CALL(*payload, Read).WillOnce([](absl::Span<char> const&) {
    return Status{StatusCode::kAborted, "error", {}};
  });
  auto result = ReadAll(std::move(payload));
  EXPECT_THAT(result.status().code(), Eq(StatusCode::kAborted));
  EXPECT_THAT(result.status().message(), Eq("error"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
