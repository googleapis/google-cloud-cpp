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

#include "google/cloud/internal/detect_gcp_impl.h"
#include "google/cloud/internal/filesystem.h"
#include "google/cloud/internal/random.h"
#include "absl/strings/string_view.h"
#include <gmock/gmock.h>
#include <fstream>
#include <vector>
#include <stdlib.h>
#ifdef _WIN32
#include <Windows.h>
#endif

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

#ifdef _WIN32
std::string const& parent_key = "SOFTWARE\\GoogleCloudCpp";
std::string const& sub_key = "SOFTWARE\\GoogleCloudCpp\\Test";
std::string const& value_key = "TestProductName";

void WriteTestRegistryValue(std::string value) {
  HKEY hKey;

  LONG result = ::RegCreateKeyExA(HKEY_CURRENT_USER, sub_key.c_str(), 0,
                                  nullptr, REG_OPTION_NON_VOLATILE,
                                  KEY_ALL_ACCESS, nullptr, &hKey, nullptr);
  if (result != ERROR_SUCCESS) return;
  result =
      ::RegSetValueExA(hKey, value_key.c_str(), 0, REG_SZ,
                       (LPBYTE)value.c_str(), (DWORD)strlen(value.c_str()) + 1);
  if (result != ERROR_SUCCESS) return;
  ::RegCloseKey(hKey);
}

void CleanupTestRegistryValue() {
  LONG result =
      ::RegDeleteKeyExA(HKEY_CURRENT_USER, sub_key.c_str(), KEY_ALL_ACCESS, 0);
  if (result != ERROR_SUCCESS) return;
  ::RegDeleteKeyExA(HKEY_CURRENT_USER, parent_key.c_str(), KEY_ALL_ACCESS, 0);
}
#else  // _WIN32
std::string TempFileName() {
  static auto generator =
      google::cloud::internal::DefaultPRNG(std::random_device{}());
  return google::cloud::internal::PathAppend(
      ::testing::TempDir(),
      ::google::cloud::internal::Sample(
          generator, 16, "abcdefghijlkmnopqrstuvwxyz0123456789"));
}
#endif

std::vector<std::string> env_vars = {"TEST_VAR_ONE"};

TEST(DetectGcpPlatform, BiosValueDoesNotExist) {
  auto detector_config =
      ::google::cloud::internal::GcpDetector::GcpDetectorConfig();

#ifdef _WIN32
  config.key = HKEY_CURRENT_USER;
  config.sub_key = sub_key;
  config.value_key = value_key;
#else  // _WIN32
  auto const file_name = TempFileName();
  detector_config.path = file_name;
#endif

  auto gcp_detector =
      ::google::cloud::internal::GcpDetectorImpl(detector_config);
  auto is_cloud_bios = gcp_detector.IsGoogleCloudBios();

  EXPECT_FALSE(is_cloud_bios);
}

TEST_P(MultiValidValuesTest, ContainsGoogleBios) {
  auto detector_config =
      ::google::cloud::internal::GcpDetector::GcpDetectorConfig();
  auto cur_param = GetParam();

#ifdef _WIN32
  WriteTestRegistryValue(std::string{cur_param});

  config.key = HKEY_CURRENT_USER;
  config.sub_key = sub_key;
  config.value_key = value_key;
#else  // _WIN32
  auto const file_name = TempFileName();

  std::ofstream(file_name) << cur_param;
  detector_config.path = file_name;
#endif

  auto gcp_detector =
      ::google::cloud::internal::GcpDetectorImpl(detector_config);
  auto is_cloud_bios = gcp_detector.IsGoogleCloudBios();

#ifdef _WIN32
  CleanupTestRegistryValue();
#else  // _WIN32
  (void)std::remove(file_name.c_str());
#endif

  EXPECT_TRUE(is_cloud_bios);
}

TEST_P(MultiInvalidValuesTest, DoesNotContainGoogleBios) {
  auto detector_config =
      ::google::cloud::internal::GcpDetector::GcpDetectorConfig();
  auto cur_param = GetParam();

#ifdef _WIN32
  WriteTestRegistryValue(std::string{cur_param});

  config.key = HKEY_CURRENT_USER;
  config.sub_key = sub_key;
  config.value_key = value_key;
#else
  auto const file_name = TempFileName();

  std::ofstream(file_name) << cur_param;
  detector_config.path = file_name;
#endif

  auto gcp_detector =
      ::google::cloud::internal::GcpDetectorImpl(detector_config);
  auto is_cloud_bios = gcp_detector.IsGoogleCloudBios();

#ifdef _WIN32
  CleanupTestRegistryValue();
#else  // _WIN32
  (void)std::remove(file_name.c_str());
#endif

  EXPECT_FALSE(is_cloud_bios);
}

TEST(DetectGcpPlatform, NoEnvVarSet) {
  auto detector_config =
      ::google::cloud::internal::GcpDetector::GcpDetectorConfig();
  detector_config.env_variables = env_vars;

  auto gcp_detector =
      ::google::cloud::internal::GcpDetectorImpl(detector_config);
  auto is_cloud_serverless = gcp_detector.IsGoogleCloudServerless();

  EXPECT_FALSE(is_cloud_serverless);
}

TEST(DetectGcpPlatform, EnvVarSet) {
  auto detector_config =
      ::google::cloud::internal::GcpDetector::GcpDetectorConfig();
  detector_config.env_variables = env_vars;

#ifdef _WIN32
  _putenv_s(env_vars[0].c_str(), "VALUE");
#else  // _WIN32
  setenv(env_vars[0].c_str(), "VALUE", false);
#endif

  auto gcp_detector =
      ::google::cloud::internal::GcpDetectorImpl(detector_config);
  auto is_cloud_serverless = gcp_detector.IsGoogleCloudServerless();

#ifdef _WIN32
  _putenv_s(env_vars[0].c_str(), "");
#else  // _WIN32
  unsetenv(env_vars[0].c_str());
#endif

  EXPECT_TRUE(is_cloud_serverless);
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
