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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_CHANNEL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_CHANNEL_H

#include "google/cloud/spanner/internal/spanner_stub.h"
#include "google/cloud/spanner/version.h"
#include <memory>

namespace google {
namespace cloud {
namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {

/**
 * `Channel` represents a single gRPC Channel/Stub.
 */
struct Channel {
  /// @p stub_param must not be nullptr
  explicit Channel(std::shared_ptr<SpannerStub> stub_param)
      : stub(std::move(stub_param)) {}

  // This class is not copyable or movable.
  Channel(Channel const&) = delete;
  Channel& operator=(Channel const&) = delete;

  std::shared_ptr<SpannerStub> const stub;
  int session_count = 0;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_CHANNEL_H
