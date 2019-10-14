// Copyright 2019 Google LLC
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

#ifndef BIGQUERY_INTERNAL_BIGQUERY_READ_STUB_H_
#define BIGQUERY_INTERNAL_BIGQUERY_READ_STUB_H_

#include <google/cloud/bigquery/storage/v1beta1/storage.pb.h>
#include <memory>

#include "google/cloud/bigquery/connection.h"
#include "google/cloud/bigquery/connection_options.h"
#include "google/cloud/bigquery/internal/stream_reader.h"
#include "google/cloud/bigquery/version.h"
#include "google/cloud/status_or.h"

namespace google {
namespace cloud {
namespace bigquery {
inline namespace BIGQUERY_CLIENT_NS {
namespace internal {

// BigQueryStorageStub is a thin stub layer over the BigQuery Storage API
// that hides the underlying transport stub, e.g., gRPC.
class BigQueryStorageStub {
 public:
  virtual ~BigQueryStorageStub() = default;

  // Sends a CreateReadSession RPC.
  virtual google::cloud::StatusOr<
      google::cloud::bigquery::storage::v1beta1::ReadSession>
  CreateReadSession(
      google::cloud::bigquery::storage::v1beta1::CreateReadSessionRequest const&
          request) = 0;

  virtual std::unique_ptr<
      StreamReader<google::cloud::bigquery::storage::v1beta1::ReadRowsResponse>>
  ReadRows(google::cloud::bigquery::storage::v1beta1::ReadRowsRequest const&
               request) = 0;

 protected:
  BigQueryStorageStub() = default;
};

std::shared_ptr<BigQueryStorageStub> MakeDefaultBigQueryStorageStub(
    ConnectionOptions const& options);

}  // namespace internal
}  // namespace BIGQUERY_CLIENT_NS
}  // namespace bigquery
}  // namespace cloud
}  // namespace google

#endif  // BIGQUERY_INTERNAL_BIGQUERY_READ_STUB_H_
