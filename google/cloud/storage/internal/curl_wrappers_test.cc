// Copyright 2020 Google LLC
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

#include "google/cloud/storage/internal/curl_wrappers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

TEST(CurlWrappersTest, ExtractUrlHostpart) {
  struct Test {
    std::string expected;
    std::string input;
  } cases[] = {
      {"storage.googleapis.com", "https://storage.googleapis.com"},
      {"storage.googleapis.com", "https://storage.googleapis.com"},
      {"storage.googleapis.com", "https://storage.googleapis.com:443/"},
      {"localhost", "http://localhost/"},
      {"localhost", "http://localhost/"},
      {"localhost", "http://localhost:8080/"},
      {"localhost", "http://localhost/foo/bar"},
      {"localhost", "http://localhost:8080/foo/bar"},
      {"localhost", "http://localhost:8080/foo/bar"},
      {"::1", "http://[::1]"},
      {"::1", "http://[::1]/"},
      {"::1", "http://[::1]:8080/"},
      {"::1", "http://[::1]/foo/bar"},
      {"::1", "http://[::1]:8080/foo/bar"},
      {"127.0.0.1", "http://127.0.0.1"},
      {"127.0.0.1", "http://127.0.0.1/"},
      {"127.0.0.1", "http://127.0.0.1:8080"},
      {"127.0.0.1", "http://127.0.0.1:8080/"},
      {"127.0.0.1", "http://127.0.0.1/foo/bar"},
      {"127.0.0.1", "http://127.0.0.1:8080/foo/bar"},
      {"storage-download.127.0.0.1.nip.io",
       "https://storage-download.127.0.0.1.nip.io/xmlapi/"},
      {"gcs.127.0.0.1.nip.io", "https://gcs.127.0.0.1.nip.io/storage/v1/"},
      {"gcs.127.0.0.1.nip.io",
       "https://gcs.127.0.0.1.nip.io:4443/upload/storage/v1/"},
      {"gcs.127.0.0.1.nip.io",
       "https://gcs.127.0.0.1.nip.io:4443/upload/storage/v1/"},
  };

  for (auto const& t : cases) {
    EXPECT_EQ(t.expected, ExtractUrlHostpart(t.input));
  }
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
