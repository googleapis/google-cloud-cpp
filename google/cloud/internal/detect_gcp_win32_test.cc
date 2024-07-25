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

#ifdef _WIN32

#include "google/cloud/internal/detect_gcp_win32.h"
#include "google/cloud/internal/filesystem.h"
#include "google/cloud/internal/random.h"
#include "absl/strings/string_view.h"
#include <gmock/gmock.h>
#include <fstream>
#include <Windows.h>
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


std::string const& parent_key = "SOFTWARE\\GoogleCloudCpp";
std::string const& sub_key = "SOFTWARE\\GoogleCloudCpp\\Test";
std::string const& value_key = "TestProductName";
std::vector<std::string> env_vars = {"TEST_VAR_ONE"};

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

TEST(DetectGcpPlatform, RegistryValueDoesNotExist) {
  auto gcp_detector = ::google::cloud::internal::GcpDetectorWin32Impl();
  auto is_google_bios = gcp_detector.IsGoogleCloudBios(HKEY_CURRENT_USER,
                                                         sub_key, value_key);

  EXPECT_FALSE(is_google_bios);
}

TEST_P(MultiValidValuesTest, RegistryContainsGcpValue) {
  auto cur_param = GetParam();
  WriteTestRegistryValue(std::string{cur_param});

  auto gcp_detector = ::google::cloud::internal::GcpDetectorWin32Impl();
  auto is_google_bios = gcp_detector.IsGoogleCloudBios(HKEY_CURRENT_USER,
                                                         sub_key, value_key);
  CleanupTestRegistryValue();

  EXPECT_TRUE(is_google_bios);
}

TEST_P(MultiInvalidValuesTest, RegistryDoesNotContainGcpValue) {
  auto cur_param = GetParam();
  WriteTestRegistryValue(std::string{cur_param});

  auto gcp_detector = ::google::cloud::internal::GcpDetectorWin32Impl();
  auto is_google_bios = gcp_detector.IsGoogleCloudBios(HKEY_CURRENT_USER,
                                                         sub_key, value_key);
  CleanupTestRegistryValue();

  EXPECT_FALSE(is_google_bios);
}

TEST(DetectGcpPlatform, NoEnvVarSet) {
  auto gcp_detector = ::google::cloud::internal::GcpDetectorWin32Impl();
  auto is_cloud_serverless = gcp_detector.IsGoogleCloudServerless(env_vars);

  EXPECT_FALSE(is_cloud_serverless);
}

TEST(DetectGcpPlatform, EnvVarSet) {
  _putenv_s(env_vars[0].c_str(), "VALUE");

  auto gcp_detector = ::google::cloud::internal::GcpDetectorWin32Impl();
  auto is_cloud_serverless = gcp_detector.IsGoogleCloudServerless(env_vars);

  _putenv_s(env_vars[0].c_str(), "");

  EXPECT_TRUE(is_cloud_serverless);
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // _WIN32
