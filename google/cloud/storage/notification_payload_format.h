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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_NOTIFICATION_PAYLOAD_FORMAT_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_NOTIFICATION_PAYLOAD_FORMAT_H_

#include "google/cloud/storage/version.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
/// Contains functions to return well-known notification payload format names.
namespace payload_format {
inline char const* JsonApiV1() {
  static constexpr char kPayloadFormat[] = "JSON_API_V1";
  return kPayloadFormat;
}

inline char const* None() {
  static constexpr char kPayloadFormat[] = "NONE";
  return kPayloadFormat;
}

}  // namespace payload_format
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_NOTIFICATION_PAYLOAD_FORMAT_H_
