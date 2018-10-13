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

#include "google/cloud/storage/internal/hash_validator.h"
#include "google/cloud/storage/status.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

TEST(NullHashValidatorTest, Simple) {
  NullHashValidator validator;
  validator.ProcessHeader("x-goog-hash", "md5=<placeholder-for-test>");
  validator.Update("The quick");
  validator.Update(" brown");
  validator.Update(" fox jumps over the lazy dog");
  auto result = std::move(validator).Finish("test msg");
  EXPECT_TRUE(result.computed.empty());
  EXPECT_TRUE(result.received.empty());
}

TEST(MD5HashValidator, Empty) {
  MD5HashValidator validator;
  // Use this command to get the expected value:
  // /bin/echo -n "" | openssl md5 -binary | openssl base64
  validator.ProcessHeader(
      "x-goog-hash", "md5=1B2M2Y8AsgTpgAmY7PhCfg==");
  auto result = std::move(validator).Finish("test msg");
  EXPECT_EQ(result.computed, result.received);
  EXPECT_EQ("1B2M2Y8AsgTpgAmY7PhCfg==", result.computed);
}

TEST(MD5HashValidator, Simple) {
  MD5HashValidator validator;
  validator.Update("The quick");
  validator.Update(" brown");
  validator.Update(" fox jumps over the lazy dog");
  validator.ProcessHeader(
      "x-goog-hash", "crc32c=<should-be-ignored>,md5=<invalid-value-for-test>");
  // I used this command to get the expected value:
  // /bin/echo -n "The quick brown fox jumps over the lazy dog" |
  //     openssl md5 -binary | openssl base64
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(try {
    std::move(validator).Finish("test msg");
  } catch(google::cloud::storage::HashMismatchError const& ex) {
    EXPECT_EQ("<invalid-value-for-test>", ex.received_hash());
    EXPECT_EQ("nhB9nTcrtoJr2B01QqQZ1g==", ex.computed_hash());
    throw;
  }, std::ios::failure);
#else
  auto result = std::move(validator).Finish("test msg");
  EXPECT_EQ("<invalid-value-for-test>", result.received);
  EXPECT_EQ("nhB9nTcrtoJr2B01QqQZ1g==", result.computed);
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
