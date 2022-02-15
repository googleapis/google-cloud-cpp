// Copyright 2021 Google LLC
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

#include "google/cloud/bigquery/bigquery_read_connection.h"
#include "google/cloud/bigquery/internal/bigquery_write_connection_impl.h"
#include "google/cloud/version.h"

namespace google {
namespace cloud {
namespace bigquery_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

void BigQueryReadReadRowsStreamingUpdater(
    ::google::cloud::bigquery::storage::v1::ReadRowsResponse const& response,
    ::google::cloud::bigquery::storage::v1::ReadRowsRequest& request) {
  request.set_offset(request.offset() + response.row_count());
}

std::unique_ptr<::google::cloud::AsyncStreamingReadWriteRpc<
    google::cloud::bigquery::storage::v1::AppendRowsRequest,
    google::cloud::bigquery::storage::v1::AppendRowsResponse>>
BigQueryWriteConnectionImpl::AsyncAppendRows(ExperimentalTag) {
  return stub_->AsyncAppendRows(background_->cq(),
                                absl::make_unique<grpc::ClientContext>());
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_internal
}  // namespace cloud
}  // namespace google
