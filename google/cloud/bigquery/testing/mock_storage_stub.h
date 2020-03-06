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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_TESTING_MOCK_STORAGE_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_TESTING_MOCK_STORAGE_STUB_H

#include "google/cloud/bigquery/internal/storage_stub.h"
#include "google/cloud/bigquery/internal/stream_reader.h"
#include "google/cloud/bigquery/version.h"
#include "google/cloud/status_or.h"
#include <google/cloud/bigquery/storage/v1beta1/storage.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_testing {
inline namespace BIGQUERY_CLIENT_NS {
class MockStorageStub : public google::cloud::bigquery::internal::StorageStub {
 public:
  MOCK_METHOD1(CreateReadSession,
               google::cloud::StatusOr<
                   google::cloud::bigquery::storage::v1beta1::ReadSession>(
                   google::cloud::bigquery::storage::v1beta1::
                       CreateReadSessionRequest const&));

  MOCK_METHOD1(
      ReadRows,
      std::unique_ptr<bigquery::internal::StreamReader<
          google::cloud::bigquery::storage::v1beta1::ReadRowsResponse>>(
          google::cloud::bigquery::storage::v1beta1::ReadRowsRequest const&));
};

}  // namespace BIGQUERY_CLIENT_NS
}  // namespace bigquery_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_TESTING_MOCK_STORAGE_STUB_H
