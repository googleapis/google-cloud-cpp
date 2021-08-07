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

#include "google/cloud/storage/internal/curl_download_request.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

TEST(CurlDownloadRequest, ExtractHashValue) {
  struct Test {
    std::string value;
    std::string key;
    std::string expected;
  } cases[] = {
      {"", "", ""},
      {"crc32c=123", "crc32c=", "123"},
      {"crc32c=123,", "crc32c=", "123"},
      {"crc32c=", "crc32c=", ""},
      {"crc32c=,", "crc32c=", ""},
      {"crc32c=123, md4=abc", "crc32c=", "123"},
      {"md5=abc, crc32c=123", "crc32c=", "123"},
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("Testing with " + test.value + " and " + test.key);
    auto const actual = ExtractHashValue(test.value, test.key);
    EXPECT_EQ(test.expected, actual);
  }
}

TEST(CurlDownloadRequest, MakeReadResult) {
  struct Test {
    std::string name;
    std::multimap<std::string, std::string> headers;
    HashValues expected_hashes;
    absl::optional<std::int64_t> expected_generation;
  } cases[] = {
      {"empty", {}, {}, absl::nullopt},
      {"irrelevant headers",
       {{"x-generation", "123"},
        {"x-goog-stuff", "thing"},
        {"x-hashes", "crc32c=123"}},
       {},
       absl::nullopt},
      {"generation", {{"x-goog-generation", "123"}}, {}, 123},
      {"hashes",
       {{"x-goog-hash", "md5=123, crc32c=abc"}},
       {"abc", "123"},
       absl::nullopt},
      {"split hashes",
       {{"x-goog-hash", "md5=123"}, {"x-goog-hash", "crc32c=abc"}},
       {"abc", "123"},
       absl::nullopt},
      {"hashes and generation",
       {{"x-goog-hash", "md5=123, crc32c=abc"}, {"x-goog-generation", "456"}},
       {"abc", "123"},
       456},
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("Test case: " + test.name);
    auto const actual = MakeReadResult(42, HttpResponse{200, {}, test.headers});
    EXPECT_EQ(42, actual.bytes_received);
    EXPECT_EQ(200, actual.response.status_code);
    EXPECT_EQ(test.expected_generation, actual.generation);
    EXPECT_EQ(test.expected_hashes.crc32c, actual.hashes.crc32c);
    EXPECT_EQ(test.expected_hashes.md5, actual.hashes.md5);
  }
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
