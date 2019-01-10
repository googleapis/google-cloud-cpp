// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/signed_url_requests.h"
#include "google/cloud/storage/internal/parse_rfc3339.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
using ::testing::HasSubstr;

TEST(SignedUrlRequests, Sign) {
  SignUrlRequest request("GET", "test-bucket", "test-object");
  EXPECT_EQ("GET", request.verb());
  EXPECT_EQ("test-bucket", request.bucket_name());
  EXPECT_EQ("test-object", request.object_name());

  request.set_multiple_options(
      ExpirationTime(ParseRfc3339("2018-12-03T12:00:00Z")),
      MD5HashValue("rmYdCNHKFXam78uCt7xQLw=="), ContentType("text/plain"),
      AddExtensionHeader("x-goog-meta-foo", "bar"),
      AddExtensionHeader("x-goog-meta-foo", "baz"),
      AddExtensionHeader("x-goog-encryption-algorithm", "AES256"),
      AddExtensionHeader("x-goog-acl", "project-private"),
      WithUserProject("test-project"));

  // Used this command to get the date in seconds:
  //   date +%s -u --date=2018-12-03T12:00:00Z
  std::string expected_blob = R"""(GET
rmYdCNHKFXam78uCt7xQLw==
text/plain
1543838400
x-goog-acl:project-private
x-goog-encryption-algorithm:AES256
x-goog-meta-foo:bar,baz
/test-bucket/test-object?userProject=test-project)""";

  EXPECT_EQ(expected_blob, request.StringToSign());

  std::ostringstream os;
  os << request;
  EXPECT_THAT(os.str(), HasSubstr(expected_blob));
}

TEST(SignedUrlRequests, SignEscaped) {
  SignUrlRequest request("GET", "test-bucket", "test- -?-+-/-:-&-object");
  EXPECT_EQ("GET", request.verb());
  EXPECT_EQ("test-bucket", request.bucket_name());
  EXPECT_EQ("test- -?-+-/-:-&-object", request.object_name());

  request.set_multiple_options(
      ExpirationTime(ParseRfc3339("2018-12-03T12:00:00Z")),
      WithMarker("foo+bar"));

  // Used this command to get the date in seconds:
  //   date +%s -u --date=2018-12-03T12:00:00Z
  std::string expected_blob = R"""(GET


1543838400
/test-bucket/test-%20-%3F-%2B-%2F-%3A-%26-object?marker=foo%2Bbar)""";

  EXPECT_EQ(expected_blob, request.StringToSign());

  std::ostringstream os;
  os << request;
  EXPECT_THAT(os.str(), HasSubstr(expected_blob));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
