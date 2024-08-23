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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_DETECT_GCP_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_DETECT_GCP_IMPL_H

#include "google/cloud/internal/detect_gcp.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <memory>
#include <string>
#include <vector>
#ifdef _WIN32
#include <wtypes.h>
#endif

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

class GcpDetectorImpl : public GcpDetector {
 public:
#ifdef _WIN32
  struct GcpDetectorConfig {
    HKEY key;
    std::string sub_key;
    std::string value_key;
    std::vector<std::string> env_variables;
  };
#else  // _WIN32
  struct GcpDetectorConfig {
    std::string path;
    std::vector<std::string> env_variables;
  };
#endif
  explicit GcpDetectorImpl(GcpDetectorConfig config)
      : config_(std::move(config)) {};
  bool IsGoogleCloudBios() override;
  bool IsGoogleCloudServerless() override;

 private:
  StatusOr<std::string> GetBiosInformation() const;
  GcpDetectorConfig config_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_DETECT_GCP_IMPL_H
