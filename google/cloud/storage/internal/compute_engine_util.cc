// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/compute_engine_util.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/log.h"
#include <string>
#if _WIN32
#include <Windows.h>
#else  // On Linux
#include <fstream>
#endif  // _WIN32

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

std::string GceMetadataHostname() {
  auto maybe_hostname =
      google::cloud::internal::GetEnv(GceMetadataHostnameEnvVar());
  if (maybe_hostname.has_value()) {
    return *maybe_hostname;
  }
  return "metadata.google.internal";
}

bool RunningOnComputeEngineVm() {
  // Allow overriding this value for integration tests.
  auto override_val = google::cloud::internal::GetEnv(GceCheckOverrideEnvVar());
  if (override_val.has_value()) {
    return std::string("1") == *override_val;
  }

#if _WIN32
  // These values came from a GCE VM running Windows Server 2012 R2.
  std::wstring const reg_key_path = L"SYSTEM\\HardwareConfig\\Current\\";
  std::wstring const reg_key_name = L"SystemProductName";
  std::wstring const gce_product_name = L"Google Compute Engine";

  // Get the size of the string first to allocate our buffer. This includes
  // enough space for the trailing NUL character that will be included.
  DWORD buffer_size{};
  auto rc = ::RegGetValueW(
      HKEY_LOCAL_MACHINE, reg_key_path.c_str(), reg_key_name.c_str(),
      RRF_RT_REG_SZ,
      nullptr,        // We know the type will be REG_SZ.
      nullptr,        // We're only fetching the size; no buffer given yet.
      &buffer_size);  // Fetch the size in bytes of the value, if it exists.
  if (rc != 0) {
    return false;
  }

  // Allocate the buffer size and retrieve the product name string.
  std::wstring buffer;
  buffer.resize(static_cast<std::size_t>(buffer_size) / sizeof(wchar_t));
  rc = ::RegGetValueW(
      HKEY_LOCAL_MACHINE, reg_key_path.c_str(), reg_key_name.c_str(),
      RRF_RT_REG_SZ,
      nullptr,                         // We know the type will be REG_SZ.
      static_cast<void*>(&buffer[0]),  // Fetch the string value this time.
      &buffer_size);  // The wstring size in bytes, not including trailing NUL.
  if (rc != 0) {
    return false;
  }

  // Account for the trailing NUL character in the retrieved value, along with
  // the additional NUL char appended to all wstring objects.
  buffer_size = static_cast<std::size_t>(buffer_size) / sizeof(wchar_t);
  if (buffer_size == 0) {
    return false;
  }
  buffer_size--;
  buffer.resize(static_cast<std::size_t>(buffer_size));

  return buffer == gce_product_name;
#else   // Running on Linux
  // On Linux GCE VMs, we expect to see "Google Compute Engine" as the contents
  // of the file at /sys/class/dmi/id/product_name.
  std::string const gce_product_name = "Google Compute Engine";
  std::string const product_name_file = "/sys/class/dmi/id/product_name";
  std::ifstream is(product_name_file);
  if (not is.is_open()) {
    GCP_LOG(WARNING) << "Could not find file '" << product_name_file
                     << "' when checking if running on GCE, returning false";
    return false;
  }
  std::string first_line;
  std::getline(is, first_line);
  return first_line == gce_product_name;
#endif  // _WIN32
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
