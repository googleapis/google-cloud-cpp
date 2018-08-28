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

#include "google/cloud/storage/version.h"
#include <curl/curl.h>
#include <gmock/gmock.h>
#include <openssl/crypto.h>
#include <openssl/opensslv.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
using ::testing::StartsWith;

/// @test A trivial test for the Google Cloud Storage C++ Client
TEST(LinkTest, Simple) {
  EXPECT_FALSE(version_string().empty());
  EXPECT_EQ(STORAGE_CLIENT_VERSION_MAJOR, version_major());
  EXPECT_EQ(STORAGE_CLIENT_VERSION_MINOR, version_minor());
  EXPECT_EQ(STORAGE_CLIENT_VERSION_PATCH, version_patch());
}

/// @test Verify that the OpenSSL and CURL libraries linked are compatible.
TEST(LinkTest, CurlVsOpenSSL) {
  auto v = curl_version_info(CURLVERSION_NOW);
  std::string curl_ssl = v->ssl_version;
  std::string expected_prefix = curl_ssl;
  std::transform(expected_prefix.begin(), expected_prefix.end(),
                 expected_prefix.begin(),
                 [](char x) { return x == '/' ? ' ' : x; });
#ifdef OPENSSL_VERSION
  std::string openssl_v = OpenSSL_version(OPENSSL_VERSION);
#else
  std::string openssl_v = SSLeay_version(SSLEAY_VERSION);
#endif  // OPENSSL_VERSION
  EXPECT_THAT(openssl_v, StartsWith(expected_prefix))
      << "Mismatched versions of OpenSSL linked in libcurl vs. the version"
      << " linked by the Google Cloud Storage C++ library.\n"
      << "libcurl is linked against " << curl_ssl
      << "\nwhile the google cloud storage library links against " << openssl_v
      << "\nMismatched versions are not supported.  The Google Cloud Storage"
      << "\nC++ library needs to configure the OpenSSL library used by libcurl"
      << "\nand this is not possible if you link different versions.";
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
