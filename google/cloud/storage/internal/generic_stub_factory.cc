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

#include "google/cloud/storage/internal/generic_stub_factory.h"
#include "google/cloud/storage/internal/logging_stub.h"
#include "google/cloud/storage/internal/rest/stub.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/internal/algorithm.h"
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

bool RequiresLogging(Options const& opts) {
  using ::google::cloud::internal::Contains;
  auto const& tracing_components = opts.get<LoggingComponentsOption>();
  return Contains(tracing_components, "raw-client") ||
         Contains(tracing_components, "rpc");
}

std::unique_ptr<GenericStub> DecorateStub(bool logging,
                                          std::unique_ptr<GenericStub> stub) {
  if (logging) {
    stub = std::make_unique<storage::internal::LoggingStub>(std::move(stub));
  }
  return stub;
}

}  // namespace

std::unique_ptr<GenericStub> DecorateStub(Options const& opts,
                                          std::unique_ptr<GenericStub> stub) {
  return DecorateStub(RequiresLogging(opts), std::move(stub));
}

std::unique_ptr<GenericStub> MakeDefaultStorageStub(Options opts) {
  auto const logging = RequiresLogging(opts);
  return DecorateStub(
      logging, std::make_unique<storage::internal::RestStub>(std::move(opts)));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
