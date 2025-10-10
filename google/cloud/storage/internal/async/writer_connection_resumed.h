// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_WRITER_CONNECTION_RESUMED_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_WRITER_CONNECTION_RESUMED_H

#include "google/cloud/storage/async/writer_connection.h"
#include "google/cloud/storage/internal/async/handle_redirect_error.h"
#include "google/cloud/storage/internal/async/write_object.h"
#include "google/cloud/storage/internal/async/writer_connection_impl.h"
#include "google/cloud/future.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <cstddef>
#include <functional>
#include <memory>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using WriterResultFactory =
    std::function<future<StatusOr<WriteObject::WriteResult>>(
        google::storage::v2::BidiWriteObjectRequest)>;

std::unique_ptr<storage_experimental::AsyncWriterConnection>
MakeWriterConnectionResumed(
    WriterResultFactory factory,
    std::unique_ptr<storage_experimental::AsyncWriterConnection> impl,
    google::storage::v2::BidiWriteObjectRequest initial_request,
    std::shared_ptr<storage::internal::HashFunction> hash_function,
    google::storage::v2::BidiWriteObjectResponse const& first_response,
    Options const& options);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_WRITER_CONNECTION_RESUMED_H
