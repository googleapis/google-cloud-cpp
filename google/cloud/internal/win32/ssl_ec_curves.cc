// Copyright 2026 Google LLC
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
#include "google/cloud/internal/ssl_ec_curves.h"

// windows.h must be included before ncrypt.h
// clang-format off
#include <windows.h>
#include <ncrypt.h>
// clang-format on

#include <utility>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::vector<std::string> AvailableCryptoGroups() {
  std::vector<std::string> groups;
  NCRYPT_PROV_HANDLE hProvider = 0;
  if (FAILED(NCryptOpenStorageProvider(&hProvider, nullptr, 0))) return groups;

  DWORD count = 0;
  NCryptAlgorithmName* alg_list = nullptr;
  if (SUCCEEDED(NCryptEnumAlgorithms(hProvider,
                                     NCRYPT_SECRET_AGREEMENT_OPERATION, &count,
                                     &alg_list, 0))) {
    for (DWORD i = 0; i < count; ++i) {
      int size = WideCharToMultiByte(CP_UTF8, 0, alg_list[i].pszName, -1,
                                     nullptr, 0, nullptr, nullptr);
      if (size > 1) {
        std::string name(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, alg_list[i].pszName, -1, &name[0], size,
                            nullptr, nullptr);
        name.resize(size - 1);
        groups.push_back(std::move(name));
      }
    }
    NCryptFreeBuffer(alg_list);
  }
  NCryptFreeObject(hProvider);
  return groups;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
#endif  // _WIN32
