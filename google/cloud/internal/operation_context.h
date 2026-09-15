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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OPERATION_CONTEXT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OPERATION_CONTEXT_H

#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * An abstract base class for service-specific operation contexts across
 * retries.
 *
 * This provides lifecycle hooks (`PreCall`, `PostCall`, `OnDone`) for generated
 * stubs and decorators.
 */
class OperationContext {
 public:
  virtual ~OperationContext() = default;

  // Called before each RPC attempt to inject headers and update state.
  virtual void PreCall(grpc::ClientContext& context) = 0;

  // Called after receiving an RPC attempt response.
  virtual void PostCall(grpc::ClientContext const& context,
                        Status const& status) = 0;

  // Called when the overall logical operation completes across all attempts.
  virtual void OnDone(Status const& status) = 0;
};

class NoopOperationContext : public OperationContext {
 public:
  ~NoopOperationContext() override = default;
  void PreCall(grpc::ClientContext&) override {}
  void PostCall(grpc::ClientContext const&, Status const&) override {}
  void OnDone(Status const&) override {}
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OPERATION_CONTEXT_H
