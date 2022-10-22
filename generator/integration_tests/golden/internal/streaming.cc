// Copyright 2020 Google LLC
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

#include "generator/integration_tests/golden/golden_kitchen_sink_connection.h"
#include "generator/integration_tests/golden/internal/golden_kitchen_sink_connection_impl.h"
#include "google/cloud/version.h"

namespace google {
namespace cloud {
namespace golden_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::unique_ptr<::google::cloud::AsyncStreamingReadWriteRpc<
    google::test::admin::database::v1::Request,
    google::test::admin::database::v1::Response>>
GoldenKitchenSinkConnectionImpl::AsyncStreamingReadWrite(google::cloud::ExperimentalTag) {
  return stub_->AsyncStreamingReadWrite(
      background_->cq(), absl::make_unique<grpc::ClientContext>());
}

void GoldenKitchenSinkStreamingReadStreamingUpdater(
    ::google::test::admin::database::v1::Response const&,
    ::google::test::admin::database::v1::Request&) {
  // implementation here
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_internal
}  // namespace cloud
}  // namespace google
