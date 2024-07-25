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
#include "absl/strings/ascii.h"
#include "absl/strings/string_view.h"
#include <Windows.h>
#include <stdlib.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

std::string GcpDetectorWin32Impl::GetBiosInformation(
    HKEY key, std::string const& sub_key, std::string const& value_key) {
  DWORD size{};
  LONG result = ::RegGetValueA(key, sub_key.c_str(), value_key.c_str(),
                               RRF_RT_REG_SZ, nullptr, nullptr, &size);

  if (result != ERROR_SUCCESS) return "";

  std::string contents;
  contents.resize(size / sizeof(char));
  result = ::RegGetValueA(key, sub_key.c_str(), value_key.c_str(),
                          RRF_RT_REG_SZ, nullptr, &contents[0], &size);

  if (result != ERROR_SUCCESS) return "";

  DWORD content_length = size / sizeof(char);
  content_length--;  // Exclude NUL written by WIN32
  contents.resize(content_length);

  return contents;
}

bool GcpDetectorWin32Impl::IsGoogleCloudBios(HKEY key,
                                             std::string const& sub_key,
                                             std::string const& value_key) {
  auto bios_information = this->GetBiosInformation(key, sub_key, value_key);
  absl::StripAsciiWhitespace(&bios_information);

  return bios_information == "Google" ||
         bios_information == "Google Compute Engine";
}

bool GcpDetectorWin32Impl::IsGoogleCloudServerless(
    std::vector<std::string> const& env_variables) {
  for (auto env_var : env_variables) {
    char* buf = nullptr;
    size_t size = 0;
    // Use _dupenv_s here instead of getenv. 
    // MSVC throws a security error on getenv
    auto result = _dupenv_s(&buf, &size, env_var.c_str());
    if (result == 0 && buf != nullptr) {
      free(buf);
      return true;
    }
  }

  return false;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // _WIN32
