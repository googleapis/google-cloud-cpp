// Copyright 2020 Google LLC
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

#include "google/cloud/kms_key_name.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <sstream>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;

TEST(KmsKeyNameTest, FromComponents) {
  KmsKeyName key("test-project", "some-location", "a-key-ring", "a-key-name");
  EXPECT_EQ(
      "projects/test-project/locations/some-location/keyRings/a-key-ring/"
      "cryptoKeys/a-key-name",
      key.FullName());
}

TEST(KmsKeyNameTest, MakeKmsKeyName) {
  auto key = KmsKeyName("p1", "l1", "r1", "n1");
  EXPECT_EQ(key, MakeKmsKeyName(key.FullName()).value());

  for (std::string invalid : {
           "projects/p1/locations/l1/keyRings/r1/carlosKey/n1",
           "projects/p1",
           "",
           "projects//locations/l1/keyRings/r1/cryptoKeys/n1",
           "/projects/p1/locations/l1/keyRings/r1/cryptoKeys/n1",
           "plojects/p1/locations/l1/keyRings/r1/cryptoKeys/n1",
           "projects/p1/locations/l1/keyRings/r1/cryptoKeys/n1/",
       }) {
    auto key = MakeKmsKeyName(invalid);
    EXPECT_THAT(key, StatusIs(StatusCode::kInvalidArgument,
                              "Improperly formatted KmsKeyName: " + invalid));
  }
}

TEST(KmsKeyNameTest, Equality) {
  KmsKeyName key1("proj", "loc", "ring", "keyname");
  auto key2 = MakeKmsKeyName(
      "projects/proj/locations/loc/keyRings/ring/cryptoKeys/keyname");
  ASSERT_THAT(key2, IsOk());
  EXPECT_EQ(key1, *key2);

  KmsKeyName key3("proj", "loc", "ring2", "keyname");
  EXPECT_NE(key1, key3);
  EXPECT_NE(*key2, key3);
}

TEST(Instance, OutputStream) {
  KmsKeyName key("P", "L", "R", "N");
  std::ostringstream os;
  os << key;
  EXPECT_EQ("projects/P/locations/L/keyRings/R/cryptoKeys/N", os.str());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
