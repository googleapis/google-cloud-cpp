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

#include <google/cloud/bigquery/storage/v1beta1/storage.pb.h>
#include <memory>
#include <sstream>
#include <string>

#include "google/cloud/bigquery/internal/bigquerystorage_stub.h"
#include "google/cloud/bigquery/internal/connection_impl.h"
#include "google/cloud/bigquery/version.h"
#include "google/cloud/status_or.h"

namespace google {
namespace cloud {
namespace bigquery {
inline namespace BIGQUERY_CLIENT_NS {
namespace internal {

namespace bigquerystorage_proto = ::google::cloud::bigquery::storage::v1beta1;

using ::google::cloud::Status;
using ::google::cloud::StatusCode;
using ::google::cloud::StatusOr;

namespace {
template <char delimiter>
class DelimitedBy : public std::string {};

// TODO(aryann): Replace this with absl::StrSplit once it is
// available.
template <char delimiter>
std::vector<std::string> StrSplit(std::string input) {
  std::istringstream iss(input);
  return {std::istream_iterator<DelimitedBy<delimiter>>(iss),
          std::istream_iterator<DelimitedBy<delimiter>>()};
}

template <char delimiter>
std::istream& operator>>(std::istream& is, DelimitedBy<delimiter>& output) {
  std::getline(is, output, delimiter);
  return is;
}
}  // namespace

ConnectionImpl::ConnectionImpl(std::shared_ptr<BigQueryStorageStub> read_stub)
    : read_stub_(read_stub) {}

StatusOr<std::string> ConnectionImpl::CreateSession(
    std::string parent_project_id, std::string table) {
  auto parts = StrSplit<':'>(table);
  if (parts.size() != 2) {
    return Status(
        StatusCode::kInvalidArgument,
        "Table name must be of the form PROJECT_ID:DATASET_ID.TABLE_ID.");
  }
  std::string project_id = parts[0];
  parts = StrSplit<'.'>(parts[1]);
  if (parts.size() != 2) {
    return Status(
        StatusCode::kInvalidArgument,
        "Table name must be of the form PROJECT_ID:DATASET_ID.TABLE_ID.");
  }
  std::string dataset_id = parts[0];
  std::string table_id = parts[1];

  bigquerystorage_proto::CreateReadSessionRequest request;
  request.set_parent("projects/" + parent_project_id);
  request.mutable_table_reference()->set_project_id(project_id);
  request.mutable_table_reference()->set_dataset_id(dataset_id);
  request.mutable_table_reference()->set_table_id(table_id);
  auto response = read_stub_->CreateReadSession(request);
  if (!response.ok()) {
    return response.status();
  }
  return response.value().name();
}

std::shared_ptr<ConnectionImpl> MakeConnection(
    std::shared_ptr<BigQueryStorageStub> read_stub) {
  return std::shared_ptr<ConnectionImpl>(
      new ConnectionImpl(std::move(read_stub)));
}

}  // namespace internal
}  // namespace BIGQUERY_CLIENT_NS
}  // namespace bigquery
}  // namespace cloud
}  // namespace google
