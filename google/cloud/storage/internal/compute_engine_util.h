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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_COMPUTE_ENGINE_UTIL_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_COMPUTE_ENGINE_UTIL_H_

#include "google/cloud/internal/getenv.h"
#include "google/cloud/log.h"
#include "google/cloud/storage/version.h"
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

/**
 * Returns the env var used to override the check for if we're running on GCE.
 *
 * This environment variable is used for testing to override the return value
 * for the function that checks whether we're running on a GCE VM. Some CI
 * testing services sometimes run on GCE VMs, and we don't want to accidentally
 * try to use their service account credentials during our tests.
 *
 * If set to "1", this will force `RunningOnComputeEngineVm` to return true. If
 * set to anything else, it will return false. If unset, the function will
 * actually check whether we're running on a GCE VM.
 */
inline char const* GceCheckOverrideEnvVar() {
  static constexpr char kEnvVarName[] = "GOOGLE_RUNNING_ON_GCE_CHECK_OVERRIDE";
  return kEnvVarName;
}

/**
 * Returns true if the program is running on a Compute Engine VM.
 *
 * This method checks the system BIOS information to determine if the program
 * is running on a GCE VM. This has proven to be more reliable than pinging the
 * GCE metadata server (e.g. the metadata server may be temporarily unavailable,
 * the VM may be experiencing network issues, etc.).
 */
bool RunningOnComputeEngineVm() {
  // Allow overriding this value for integration tests.
  auto override_val = google::cloud::internal::GetEnv(GceCheckOverrideEnvVar());
  if (override_val.has_value()) {
    return std::string("1") == *override_val;
  }

#if _WIN32
  // These values came from a GCE VM running Windows Server 2012 R2.
  std::wstring const REG_KEY_PATH = L"SYSTEM\\HardwareConfig\\Current\\";
  std::wstring const REG_KEY_NAME = L"SystemProductName";
  std::wstring const GCE_PRODUCT_NAME = L"Google Compute Engine";

  // Get the size of the string first to allocate our buffer. This includes
  // enough space for the trailing NUL character that will be included.
  DWORD buffer_size{};
  auto rc = ::RegGetValueW(
      HKEY_LOCAL_MACHINE, REG_KEY_PATH.c_str(), REG_KEY_NAME.c_str(),
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
      HKEY_LOCAL_MACHINE, REG_KEY_PATH.c_str(), REG_KEY_NAME.c_str(),
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

  return buffer == GCE_PRODUCT_NAME;
#else   // Running on Linux
  // On Linux GCE VMs, we expect to see "Google Compute Engine" as the contents
  // of the file at /sys/class/dmi/id/product_name.
  std::string const GCE_PRODUCT_NAME = "Google Compute Engine";
  std::string const PRODUCT_NAME_FILE = "/sys/class/dmi/id/product_name";
  std::ifstream is(PRODUCT_NAME_FILE);
  if (not is.is_open()) {
    GCP_LOG(WARNING) << "Could not find file '" << PRODUCT_NAME_FILE
                     << "' when checking if running on GCE, returning false";
    return false;
  }
  std::string first_line;
  std::getline(is, first_line);
  return first_line == GCE_PRODUCT_NAME;
#endif  // _WIN32
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_COMPUTE_ENGINE_UTIL_H_
