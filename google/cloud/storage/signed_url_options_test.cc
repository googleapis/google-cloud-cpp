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

#include "google/cloud/storage/signed_url_options.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
TEST(SignedUrlOptions, QueryParameters) {
  auto mp = [](char const* k, char const* v) {
    return std::pair<std::string, std::string>(k, v);
  };
  EXPECT_EQ("acl", WithAcl().value());
  EXPECT_EQ("billing", WithBilling().value());
  EXPECT_EQ("compose", WithCompose().value());
  EXPECT_EQ("cors", WithCors().value());
  EXPECT_EQ("encryption", WithEncryption().value());
  EXPECT_EQ("encryptionConfig", WithEncryptionConfig().value());
  EXPECT_EQ(mp("generation", "12345"), WithGeneration(12345U).value());
  EXPECT_EQ(mp("generation-marker", "23456"),
            WithGenerationMarker(23456U).value());
  EXPECT_EQ("lifecycle", WithLifecycle().value());
  EXPECT_EQ("location", WithLocation().value());
  EXPECT_EQ("logging", WithLogging().value());
  EXPECT_EQ(mp("marker", "abcd"), WithMarker("abcd").value());
  EXPECT_EQ(mp("response-content-disposition", "inline"),
            WithResponseContentDisposition("inline").value());
  EXPECT_EQ(mp("response-content-type", "text/plain"),
            WithResponseContentType("text/plain").value());
  EXPECT_EQ("storageClass", WithStorageClass().value());
  EXPECT_EQ("tagging", WithTagging().value());
  EXPECT_EQ(mp("userProject", "test-project"),
            WithUserProject("test-project").value());
}
}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
