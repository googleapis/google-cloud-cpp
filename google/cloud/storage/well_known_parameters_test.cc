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

#include "google/cloud/storage/well_known_parameters.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

TEST(PredefinedAclHeaderNameTest, Simple) {
  EXPECT_EQ("authenticated-read",
            PredefinedAcl::AuthenticatedRead().HeaderName());
  EXPECT_EQ("bucket-owner-full-control",
            PredefinedAcl::BucketOwnerFullControl().HeaderName());
  EXPECT_EQ("bucket-owner-read", PredefinedAcl::BucketOwnerRead().HeaderName());
  EXPECT_EQ("private", PredefinedAcl::Private().HeaderName());
  EXPECT_EQ("project-private", PredefinedAcl::ProjectPrivate().HeaderName());
  EXPECT_EQ("public-read", PredefinedAcl::PublicRead().HeaderName());
  EXPECT_EQ("SomeCustom", PredefinedAcl("SomeCustom").HeaderName());
}

TEST(WellKnownParameter, ValueOrEmptyCase) {
  KmsKeyName param;
  ASSERT_FALSE(param.has_value());
  EXPECT_EQ("foo", param.value_or("foo"));
}

TEST(WellKnownParameter, ValueOrNonEmptyCase) {
  KmsKeyName param("value");
  ASSERT_TRUE(param.has_value());
  EXPECT_EQ("value", param.value_or("foo"));
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
