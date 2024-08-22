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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_DETECT_GCP_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_DETECT_GCP_H

#include "google/cloud/version.h"
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * Interface for attempting to detect if running in a Google Cloud Platform
 * (GCP) environment.
 *
 * This code is split across WIN32 and other as the detection logic differs
 * slightly due to needing to make platform specific calls.
 */
class GcpDetector {
 public:
  virtual ~GcpDetector() = default;
  virtual bool IsGoogleCloudBios() = 0;
  virtual bool IsGoogleCloudServerless() = 0;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_DETECT_GCP_H
