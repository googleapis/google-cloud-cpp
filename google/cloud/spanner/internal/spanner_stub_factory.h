// Copyright 2023 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_SPANNER_STUB_FACTORY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_SPANNER_STUB_FACTORY_H

#include "google/cloud/spanner/database.h"
#include "google/cloud/spanner/internal/spanner_stub.h"
#include "google/cloud/internal/unified_grpc_credentials.h"
#include "google/cloud/options.h"
#include <memory>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::shared_ptr<SpannerStub> DecorateSpannerStub(
    std::shared_ptr<SpannerStub> stub, spanner::Database const& db,
    std::shared_ptr<internal::GrpcAuthenticationStrategy> auth,
    Options const& opts);

/**
 * Creates a SpannerStub configured with @p opts and @p channel_id.
 *
 * @p channel_id should be unique among all stubs in the same Connection pool,
 * to ensure they use different underlying connections.
 */
std::shared_ptr<SpannerStub> CreateDefaultSpannerStub(
    spanner::Database const& db,
    std::shared_ptr<internal::GrpcAuthenticationStrategy> auth,
    Options const& opts, int channel_id);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_SPANNER_STUB_FACTORY_H
