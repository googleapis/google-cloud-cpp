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
#include "google/cloud/internal/make_status.h"
#include "google/cloud/log.h"
#include "absl/strings/ascii.h"
#include "absl/strings/string_view.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
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

StatusOr<std::string> GcpDetectorImpl::GetBiosInformation() const {
#ifdef _WIN32
  DWORD size{};
  LONG result = RegGetValueA(config_.key, config_.sub_key.c_str(),
                             config_.value_key.c_str(), RRF_RT_REG_SZ, nullptr,
                             nullptr, &size);

  if (result != ERROR_SUCCESS) {
    return UnknownError("error querying registry",
                        GCP_ERROR_INFO().WithMetadata("key", std::to_string((ULONG_PTR)config_.key))
        .WithMetadata("sub_key", config_.sub_key)
        .WithMetadata("value_key", config_.value_key)
        .WithMetadata("win32_error_code", std::to_string(result)));
  }

  std::string contents;
  contents.resize(size / sizeof(char));
  result = RegGetValueA(config_.key, config_.sub_key.c_str(),
                        config_.value_key.c_str(), RRF_RT_REG_SZ, nullptr,
                        &contents[0], &size);

  if (result != ERROR_SUCCESS) {
    return UnknownError("error querying registry",
                        GCP_ERROR_INFO().WithMetadata("key", std::to_string((ULONG_PTR)config_.key))
        .WithMetadata("sub_key", config_.sub_key)
        .WithMetadata("value_key", config_.value_key)
        .WithMetadata("win32_error_code", std::to_string(result)));
  }

  DWORD content_length = size / sizeof(char);
  content_length--;  // Exclude NUL written by WIN32
  contents.resize(content_length);

  return contents;
#else  // _WIN32
  auto product_name_status = status(config_.path);
  if (!exists(product_name_status)) {
    return NotFoundError("file does not exist", GCP_ERROR_INFO().WithMetadata(
                                                    "filename", config_.path));
  }

  std::ifstream product_name_file(config_.path);
  std::string contents;
  if (!product_name_file.is_open()) {
    return UnknownError("unable to open file", GCP_ERROR_INFO().WithMetadata(
                                                   "filename", config_.path));
  }

  std::getline(product_name_file, contents);
  product_name_file.close();

  return contents;
#endif
}

bool GcpDetectorImpl::IsGoogleCloudBios() {
  auto status = GetBiosInformation();
  if (!status.ok()) {
    GCP_LOG(WARNING) << status.status();
    return false;
  }

  auto bios_information = status.value();
  absl::StripAsciiWhitespace(&bios_information);

  return bios_information == "Google" ||
         bios_information == "Google Compute Engine";
}

bool GcpDetectorImpl::IsGoogleCloudServerless() {
  return std::any_of(config_.env_variables.begin(), config_.env_variables.end(),
                     [](std::string const& env_var) {
                       return GetEnv(env_var.c_str()).has_value();
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
