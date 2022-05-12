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

#include "google/cloud/internal/curl_rest_client.h"
#include "google/cloud/common_options.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

TEST(CurlRestClientStandaloneFunctions, HostHeader) {
  struct Test {
    std::string endpoint;
    std::string authority;
    std::string expected;
  } cases[] = {
      {"https://storage.googleapis.com", "storage.googleapis.com",
       "Host: storage.googleapis.com"},
      {"https://storage.googleapis.com", "",
       "Host: storage.googleapis.com"},
      {"https://storage.googleapis.com", "auth", "Host: auth"},
      {"https://storage.googleapis.com:443", "storage.googleapis.com",
       "Host: storage.googleapis.com"},
      {"https://restricted.googleapis.com", "storage.googleapis.com",
       "Host: storage.googleapis.com"},
      {"https://private.googleapis.com", "storage.googleapis.com",
       "Host: storage.googleapis.com"},
      {"https://restricted.googleapis.com", "iamcredentials.googleapis.com",
       "Host: iamcredentials.googleapis.com"},
      {"https://private.googleapis.com", "iamcredentials.googleapis.com",
       "Host: iamcredentials.googleapis.com"},
      {"http://localhost:8080", "", ""},
      {"http://localhost:8080", "auth", "Host: auth"},
      {"http://[::1]", "", ""},
      {"http://[::1]/", "", ""},
      {"http://[::1]/foo/bar", "", ""},
      {"http://[::1]:8080/", "", ""},
      {"http://[::1]:8080/foo/bar", "", ""},
      {"http://localhost:8080", "", ""},
      {"https://storage-download.127.0.0.1.nip.io/xmlapi/", "", ""},
      {"https://gcs.127.0.0.1.nip.io/storage/v1/", "", ""},
      {"https://gcs.127.0.0.1.nip.io:4443/upload/storage/v1/", "", ""},
      {"https://gcs.127.0.0.1.nip.io:4443/upload/storage/v1/", "", ""},
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("Testing for " + test.endpoint + ", " + test.authority);
    Options options;
    if (!test.authority.empty()) options.set<AuthorityOption>(test.authority);
    auto const actual = CurlRestClient::HostHeader(options, test.endpoint);
    EXPECT_EQ(test.expected, actual);
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
