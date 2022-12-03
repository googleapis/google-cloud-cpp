// Copyright 2018 Google LLC
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

#include "google/cloud/bigtable/version.h"
#include "google/cloud/internal/build_info.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::StartsWith;

/// @test A trivial test for the Google Cloud Bigtable C++ Client
TEST(BigtableVersionTest, Simple) {
  EXPECT_FALSE(bigtable::version_string().empty());
  EXPECT_EQ(BIGTABLE_CLIENT_VERSION_MAJOR, bigtable::version_major());
  EXPECT_EQ(BIGTABLE_CLIENT_VERSION_MINOR, bigtable::version_minor());
  EXPECT_EQ(BIGTABLE_CLIENT_VERSION_PATCH, bigtable::version_patch());
}

/// @test Verify the version string starts with the version numbers.
TEST(BigtableVersionTest, Format) {
  std::ostringstream os;
  os << "v" << BIGTABLE_CLIENT_VERSION_MAJOR << "."
     << BIGTABLE_CLIENT_VERSION_MINOR << "." << BIGTABLE_CLIENT_VERSION_PATCH;
  EXPECT_THAT(version_string(), StartsWith(os.str()));
}

/// @test Verify the version contains build metadata only if defined.
TEST(BigtableVersionTest, HasMetadataWhenDefined) {
  if (google::cloud::internal::build_metadata().empty()) {
    EXPECT_THAT(version_string(), Not(HasSubstr("+")));
  } else {
    EXPECT_THAT(version_string(),
                HasSubstr("+" + google::cloud::internal::build_metadata()));
  }
}

/// @test Verify the version contains a pre-release only if defined.
TEST(BigtableVersionTest, HasPreReleaseWhenDefined) {
  char const* pre_release = version_pre_release();
  if (*pre_release == '\0') {
    EXPECT_THAT(version_string(), Not(HasSubstr("-")));
  } else {
    EXPECT_THAT(version_string(), HasSubstr(std::string{"-"} + pre_release));
  }
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
