// Copyright 2022 Google LLC
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

#include "google/cloud/storage/async/client.h"
#include "google/cloud/storage/internal/async/connection_impl.h"
#include "google/cloud/storage/internal/async/connection_tracing.h"
#include "google/cloud/storage/internal/async/default_options.h"
#include "google/cloud/storage/internal/grpc/stub.h"
#include "google/cloud/grpc_options.h"
#include <utility>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::internal::MakeBackgroundThreadsFactory;

AsyncClient::AsyncClient(Options options) {
  options = storage_internal::DefaultOptionsAsync(std::move(options));
  background_ = MakeBackgroundThreadsFactory(options)();
  connection_ = storage_internal::MakeTracingAsyncConnection(
      storage_internal::MakeAsyncConnection(background_->cq(),
                                            std::move(options)));
}

AsyncClient::AsyncClient(std::shared_ptr<AsyncConnection> connection)
    : connection_(std::move(connection)) {}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google
