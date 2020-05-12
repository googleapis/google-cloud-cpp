// Copyright 2020 Google LLC
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

#include "google/cloud/internal/strerror.h"
#include <cstdlib>
#include <cstring>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

/// A portable and thread-safe version of std::strerror()
std::string strerror(int errnum) {
  auto constexpr kMaxErrorMessageLength = 1024;
  char error_msg[kMaxErrorMessageLength];
#ifdef _WIN32
  (void)strerror_s(error_msg, sizeof(error_msg) - 1, errnum);
#else
  (void)strerror_r(errnum, error_msg, sizeof(error_msg) - 1);
#endif  // _WIN32
  return error_msg;
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
