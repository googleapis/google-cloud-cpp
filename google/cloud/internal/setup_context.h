// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SETUP_CONTEXT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SETUP_CONTEXT_H

#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/version.h"
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

/// Uses SFINAE to call Policy.Setup(context) when possible.
template <typename Policy, typename = void>
struct SetupContext {
  static void Setup(Policy&, grpc::ClientContext&) {}
};

template <typename Policy>
struct SetupContext<Policy, void_t<decltype(std::declval<Policy>().Setup(
                                std::declval<grpc::ClientContext&>()))>> {
  static void Setup(Policy& p, grpc::ClientContext& context) {
    p.Setup(context);
  }
};

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SETUP_CONTEXT_H
