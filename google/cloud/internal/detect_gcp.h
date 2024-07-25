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

#ifndef _WIN32
#include "google/cloud/version.h"
#include <string>
#include <vector>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * Interface for attempting to detect if running in a Google Cloud Platform
 * (GCP).
 *
 * This code is split across WIN32 and other as the detection logic differs
 * slightly due to needing to make platform specific calls.
 */
class GcpDetector {
 public:
  virtual bool IsGoogleCloudBios(std::string const& path) = 0;
  virtual bool IsGoogleCloudServerless(
      std::vector<std::string> const& env_variables) = 0;

 private:
  virtual std::string GetBiosInformation(std::string const& path) = 0;
};

class GcpDetectorImpl : GcpDetector {
 public:
  bool IsGoogleCloudBios(
      std::string const& path = "/sys/class/dmi/id/product_name") override;
  bool IsGoogleCloudServerless(std::vector<std::string> const& env_variables = {
                                   "CLOUD_RUN_JOB", "FUNCTION_NAME",
                                   "K_SERVICE"}) override;

 private:
  std::string GetBiosInformation(std::string const& path) override;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
#endif  // _WIN32

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_DETECT_GCP_H
