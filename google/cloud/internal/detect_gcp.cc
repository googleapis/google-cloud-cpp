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
#include "absl/strings/ascii.h"
#include "absl/strings/string_view.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

std::string GcpDetectorImpl::GetBiosInformation(std::string const& path) {
  auto product_name_status = google::cloud::internal::status(path);
  if (!google::cloud::internal::exists(product_name_status)) return "";

  std::ifstream product_name_file(path);
  std::string contents;
  if (!product_name_file.is_open()) return "";

  std::getline(product_name_file, contents);
  product_name_file.close();

  return contents;
}

bool GcpDetectorImpl::IsGoogleCloudBios(std::string const& path) {
  auto bios_information = this->GetBiosInformation(path);
  absl::StripAsciiWhitespace(&bios_information);

  return bios_information == "Google" ||
         bios_information == "Google Compute Engine";
}

bool GcpDetectorImpl::IsGoogleCloudServerless(
    std::vector<std::string> const& env_variables) {
  for (auto env_var : env_variables) {
    if (std::getenv(env_var.c_str()) != nullptr) return true;
  }

  return false;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
#endif  // _WIN32
