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

#include "google/cloud/storage/hashing_options.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

TEST(ComputeMD5HashTest, Empty) {
  std::string actual = ComputeMD5Hash("");
  // I used this command to get the expected value:
  // /bin/echo -n "" | openssl md5 -binary | openssl base64
  EXPECT_EQ("1B2M2Y8AsgTpgAmY7PhCfg==", actual);
}

TEST(ComputeMD5HashTest, Simple) {
  std::string actual = ComputeMD5Hash(
      "The quick brown fox jumps over the lazy dog");
  // I used this command to get the expected value:
  // /bin/echo -n "The quick brown fox jumps over the lazy dog" |
  //     openssl md5 -binary | openssl base64
  EXPECT_EQ("nhB9nTcrtoJr2B01QqQZ1g==", actual);
}

TEST(ComputeCrc32cChecksumTest, Empty) {
  std::string actual = ComputeCrc32cChecksum("");
  // Use this command to get the expected value:
  // echo -n '' > foo.txt && gsutil hash foo.txt
  EXPECT_EQ("AAAAAA==", actual);
}

TEST(ComputeCrc32cChecksumTest, Simple) {
  std::string actual = ComputeCrc32cChecksum(
      "The quick brown fox jumps over the lazy dog");
  // I used this command to get the expected value:
  // /bin/echo -n "The quick brown fox jumps over the lazy dog" > foo.txt &&
  // gsutil hash foo.txt
  EXPECT_EQ("ImIEBA==", actual);
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
