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

#include "google/cloud/storage/async_client.h"
#include "google/cloud/storage/internal/async_connection_impl.h"
#include "google/cloud/storage/internal/grpc_client.h"
#include "google/cloud/grpc_options.h"

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::internal::MakeBackgroundThreadsFactory;

AsyncClient::AsyncClient(
    std::shared_ptr<google::cloud::BackgroundThreads> background,
    std::shared_ptr<storage_internal::AsyncConnection> connection)
    : background_(std::move(background)), connection_(std::move(connection)) {}

AsyncClient MakeAsyncClient(Options opts) {
  auto options = storage::internal::DefaultOptionsGrpc(std::move(opts));
  auto background = MakeBackgroundThreadsFactory(options)();
  auto connection = storage_internal::MakeAsyncConnection(background->cq(),
                                                          std::move(options));
  return AsyncClient(std::move(background), std::move(connection));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google
