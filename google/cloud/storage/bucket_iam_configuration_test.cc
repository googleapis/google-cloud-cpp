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

#include "google/cloud/storage/bucket_iam_configuration.h"
#include "absl/time/time.h"
#include <gmock/gmock.h>
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::HasSubstr;
using ::testing::Not;

std::chrono::system_clock::time_point TestTimePoint() {
  absl::Time t;
  std::string error;
  absl::ParseTime(absl::RFC3339_full, "2022-10-10T12:34:56.789Z", &t, &error);
  return absl::ToChronoTime(t);
}

TEST(UniformBucketLevelAccess, IOStream) {
  auto const to_string = [&](UniformBucketLevelAccess const& rhs) {
    std::ostringstream os;
    os << rhs;
    return std::move(os).str();
  };
  auto const input = UniformBucketLevelAccess{/*.enabled=*/true,
                                              /*.locked_time=*/TestTimePoint()};
  auto const output = to_string(input);
  EXPECT_THAT(output, HasSubstr("enabled=true"));
  EXPECT_THAT(output, HasSubstr("locked_time=2022-10-10T12:34:56.789"));
}

TEST(BucketIamConfiguration, IOStream) {
  auto const to_string = [&](BucketIamConfiguration const& rhs) {
    std::ostringstream os;
    os << rhs;
    return std::move(os).str();
  };

  auto full_input = BucketIamConfiguration{
      /*.uniform_bucket_level_access=*/UniformBucketLevelAccess{
          /*.enabled=*/true, /*.locked=*/TestTimePoint()},
      /*.public_access_prevention=*/PublicAccessPreventionEnforced()};
  auto const full_output = to_string(full_input);
  EXPECT_THAT(full_output, HasSubstr("uniform_bucket_level_access="));
  EXPECT_THAT(full_output, HasSubstr("enabled=true"));
  EXPECT_THAT(full_output, HasSubstr("locked_time=2022-10-10T12:34:56.789"));
  EXPECT_THAT(full_output, HasSubstr("public_access_prevention=enforced"));

  auto no_ubla_input = BucketIamConfiguration{
      /*.uniform_bucket_level_access=*/absl::nullopt,
      /*.public_access_prevention=*/PublicAccessPreventionEnforced()};
  auto const no_ubla_output = to_string(no_ubla_input);
  EXPECT_THAT(no_ubla_output, Not(HasSubstr("uniform_bucket_level_access=")));
  EXPECT_THAT(no_ubla_output, HasSubstr("public_access_prevention=enforced"));

  auto no_pap_input = BucketIamConfiguration{
      /*.uniform_bucket_level_access=*/UniformBucketLevelAccess{
          /*.enabled=*/true, /*.locked=*/TestTimePoint()},
      /*.public_access_prevention=*/absl::nullopt};
  auto const no_pap_output = to_string(no_pap_input);
  EXPECT_THAT(no_pap_output, HasSubstr("uniform_bucket_level_access="));
  EXPECT_THAT(no_pap_output, HasSubstr("enabled=true"));
  EXPECT_THAT(no_pap_output, HasSubstr("locked_time=2022-10-10T12:34:56.789"));
  EXPECT_THAT(no_pap_output, Not(HasSubstr("public_access_prevention")));

  auto empty_input =
      BucketIamConfiguration{/*.uniform_bucket_level_access=*/absl::nullopt,
                             /*.public_access_prevention=*/absl::nullopt};
  auto const empty_output = to_string(empty_input);
  EXPECT_THAT(empty_output, Not(HasSubstr("uniform_bucket_level_access")));
  EXPECT_THAT(empty_output, Not(HasSubstr("public_access_prevention")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
