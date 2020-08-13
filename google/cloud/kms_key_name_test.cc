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

#include "google/cloud/kms_key_name.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace {

TEST(KmsKeyNameTest, FromComponents) {
  KmsKeyName key("test-project", "some-location", "a-key-ring", "a-key-name");
  EXPECT_EQ(
      "projects/test-project/locations/some-location/keyRings/a-key-ring/"
      "cryptoKeys/a-key-name",
      key.FullName());
}

TEST(KmsKeyNameTest, Equality) {
  KmsKeyName key1("proj", "loc", "ring", "keyname");
  KmsKeyName key2(
      "projects/proj/locations/loc/keyRings/ring/cryptoKeys/keyname");
  EXPECT_EQ(key1, key2);
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
