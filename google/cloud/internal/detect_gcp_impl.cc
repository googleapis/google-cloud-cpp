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
#include "google/cloud/internal/getenv.h"
#include "absl/strings/ascii.h"
#include "absl/strings/string_view.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <vector>
#ifdef _WIN32
#include <winreg.h>
#include <wtypes.h>
#endif

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

std::string GcpDetectorImpl::GetBiosInformation() {
#ifdef _WIN32
  DWORD size{};
  LONG result = RegGetValueA(this->config_.key, this->config_.sub_key.c_str(),
                             this->config_.value_key.c_str(), RRF_RT_REG_SZ,
                             nullptr, nullptr, &size);

  if (result != ERROR_SUCCESS) return "";

  std::string contents;
  contents.resize(size / sizeof(char));
  result = RegGetValueA(this->config_.key, this->config_.sub_key.c_str(),
                        this->config_.value_key.c_str(), RRF_RT_REG_SZ, nullptr,
                        &contents[0], &size);

  if (result != ERROR_SUCCESS) return "";

  DWORD content_length = size / sizeof(char);
  content_length--;  // Exclude NUL written by WIN32
  contents.resize(content_length);

  return contents;
#else  // _WIN32
  auto product_name_status =
      google::cloud::internal::status(this->config_.path);
  if (!google::cloud::internal::exists(product_name_status)) return "";

  std::ifstream product_name_file(this->config_.path);
  std::string contents;
  if (!product_name_file.is_open()) return "";

  std::getline(product_name_file, contents);
  product_name_file.close();

  return contents;
#endif
}

bool GcpDetectorImpl::IsGoogleCloudBios() {
  auto bios_information = this->GetBiosInformation();
  absl::StripAsciiWhitespace(&bios_information);

  return bios_information == "Google" ||
         bios_information == "Google Compute Engine";
}

bool GcpDetectorImpl::IsGoogleCloudServerless() {
  return std::any_of(
      this->config_.env_variables.begin(), this->config_.env_variables.end(),
      [](std::string const& env_var) {
        return google::cloud::internal::GetEnv(env_var.c_str()).has_value();
      });
}

std::shared_ptr<GcpDetector> MakeGcpDetector() {
  auto config = GcpDetectorImpl::GcpDetectorConfig{};
  config.env_variables = {"CLOUD_RUN_JOB", "FUNCTION_NAME", "K_SERVICE"};
#ifdef _WIN32
  config.key = HKEY_LOCAL_MACHINE;
  config.sub_key = "SYSTEM\\HardwareConfig\\Current";
  config.value_key = "SystemProductName";
#else  // _WIN32
  config.path = "/sys/class/dmi/id/product_name";
#endif

  return std::make_shared<GcpDetectorImpl>(config);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
