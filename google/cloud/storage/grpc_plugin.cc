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

#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/storage/internal/connection_factory.h"
#include "google/cloud/storage/internal/generic_stub_factory.h"
#include "google/cloud/storage/internal/grpc/stub.h"
#include "google/cloud/internal/getenv.h"
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

// TODO(#13857) - stop using storage_experimental::GrpcPluginOption
#include "google/cloud/internal/disable_deprecation_warnings.inc"
bool UseRest(Options const& options) {
  using ::google::cloud::internal::GetEnv;
  auto const config =
      GetEnv("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG")
          .value_or(options.get<storage_experimental::GrpcPluginOption>());
  return config == "none";
}
#include "google/cloud/internal/diagnostics_pop.inc"

}  // namespace

google::cloud::storage::Client MakeGrpcClient(Options opts) {
  if (UseRest(opts)) return google::cloud::storage::Client(std::move(opts));
  opts = google::cloud::storage_internal::DefaultOptionsGrpc(std::move(opts));
  auto stub = std::make_unique<storage_internal::GrpcStub>(opts);
  return storage::internal::ClientImplDetails::CreateWithoutDecorations(
      MakeStorageConnection(std::move(opts), std::move(stub)));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
