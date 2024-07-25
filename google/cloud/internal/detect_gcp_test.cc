// Copyright 2024 Google LLC
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

#ifndef _WIN32

#include "google/cloud/internal/detect_gcp.h"
#include "google/cloud/internal/filesystem.h"
#include "google/cloud/internal/random.h"
#include "absl/strings/string_view.h"
#include <gmock/gmock.h>
#include <fstream>
#include <vector>
#include <stdlib.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

class MultiValidValuesTest
    : public ::testing::TestWithParam<absl::string_view> {};
class MultiInvalidValuesTest
    : public ::testing::TestWithParam<absl::string_view> {};
INSTANTIATE_TEST_SUITE_P(DetectGcpPlatform, MultiValidValuesTest,
                         testing::Values("Google", "Google Compute Engine",
                                         "  Google  ",
                                         "  Google Compute Engine  "));
INSTANTIATE_TEST_SUITE_P(DetectGcpPlatform, MultiInvalidValuesTest,
                         testing::Values("Loogle", "Test", "Google K8S Engine",
                                         "Compute Engine Google"));

std::string TempFileName() {
  static auto generator =
      google::cloud::internal::DefaultPRNG(std::random_device{}());
  return google::cloud::internal::PathAppend(
      ::testing::TempDir(),
      ::google::cloud::internal::Sample(
          generator, 16, "abcdefghijlkmnopqrstuvwxyz0123456789"));
}

std::vector<std::string> env_vars = {"TEST_VAR_ONE"};

TEST(DetectGcpPlatform, FileDoesNotExist) {
  auto const file_name = TempFileName();
  auto gcp_detector = ::google::cloud::internal::GcpDetectorImpl();
  auto is_cloud_bios = gcp_detector.IsGoogleCloudBios(file_name);

  EXPECT_FALSE(is_cloud_bios);
}

TEST_P(MultiValidValuesTest, FileExistsContainsGcpValue) {
  auto const file_name = TempFileName();
  auto cur_param = GetParam();

  std::ofstream(file_name) << cur_param;

  auto gcp_detector = ::google::cloud::internal::GcpDetectorImpl();
  auto is_cloud_bios = gcp_detector.IsGoogleCloudBios(file_name);
  (void)std::remove(file_name.c_str());

  EXPECT_TRUE(is_cloud_bios);
}

TEST_P(MultiInvalidValuesTest, FileExistsDoesNotContainGcpValue) {
  auto const file_name = TempFileName();
  auto cur_param = GetParam();

  std::ofstream(file_name) << cur_param;

  auto gcp_detector = ::google::cloud::internal::GcpDetectorImpl();
  auto is_cloud_bios = gcp_detector.IsGoogleCloudBios(file_name);
  (void)std::remove(file_name.c_str());

  EXPECT_FALSE(is_cloud_bios);
}

TEST(DetectGcpPlatform, NoEnvVarSet) {
  auto gcp_detector = ::google::cloud::internal::GcpDetectorImpl();
  auto is_cloud_serverless = gcp_detector.IsGoogleCloudServerless(env_vars);

  EXPECT_FALSE(is_cloud_serverless);
}

TEST(DetectGcpPlatform, EnvVarSet) {
  setenv(env_vars[0].c_str(), "VALUE", false);

  auto gcp_detector = ::google::cloud::internal::GcpDetectorImpl();
  auto is_cloud_serverless = gcp_detector.IsGoogleCloudServerless(env_vars);

  unsetenv(env_vars[0].c_str());

  EXPECT_TRUE(is_cloud_serverless);
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // _WIN32