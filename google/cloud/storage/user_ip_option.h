// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_USER_IP_OPTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_USER_IP_OPTION_H

#include "google/cloud/storage/internal/complex_option.h"
#include "google/cloud/version.h"
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Sets the user IP on an operation for quota enforcement purposes.
 *
 * This parameter lets you enforce per-user quotas when calling the API from a
 * server-side application.
 *
 * @note The recommended practice is to use `QuotaUser`. This parameter is
 * overridden by `QuotaUser` if both are set.
 *
 * If you set this parameter to an empty string, the client library will
 * automatically select one of the user IP addresses of your server to include
 * in the request.
 *
 * @see https://cloud.google.com/apis/docs/capping-api-usage for an introduction
 *     to quotas in Google Cloud Platform.
 */
struct UserIp : public internal::ComplexOption<UserIp, std::string> {
  using ComplexOption<UserIp, std::string>::ComplexOption;
  static char const* name() { return "userIp"; }
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_USER_IP_OPTION_H
