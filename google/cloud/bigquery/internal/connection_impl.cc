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

#include "google/cloud/bigquery/internal/connection_impl.h"
#include "google/cloud/bigquery/internal/storage_stub.h"
#include "google/cloud/bigquery/internal/streaming_read_result_source.h"
#include "google/cloud/bigquery/version.h"
#include "google/cloud/status_or.h"
#include <google/cloud/bigquery/storage/v1beta1/storage.pb.h>
#include <memory>
#include <sstream>
#include <string>

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
template <char Delimiter>
class DelimitedBy : public std::string {};

// TODO(aryann): Replace this with absl::StrSplit once it is
// available.
template <char Delimiter>
std::vector<std::string> StrSplit(std::string const& input) {
  std::istringstream iss(input);
  return {std::istream_iterator<DelimitedBy<Delimiter>>(iss),
          std::istream_iterator<DelimitedBy<Delimiter>>()};
}

template <char Delimiter>
std::istream& operator>>(std::istream& is, DelimitedBy<Delimiter>& output) {
  std::getline(is, output, Delimiter);
  return is;
}
}  // namespace

ConnectionImpl::ConnectionImpl(std::shared_ptr<StorageStub> read_stub)
    : read_stub_(std::move(read_stub)) {}

ReadResult ConnectionImpl::Read(ReadStream const& read_stream) {
  bigquerystorage_proto::ReadRowsRequest request;
  request.mutable_read_position()->mutable_stream()->set_name(
      read_stream.stream_name());
  auto source = std::unique_ptr<StreamingReadResultSource>(
      new StreamingReadResultSource(read_stub_->ReadRows(request)));
  return ReadResult(std::move(source));
}

// TODO(aryann) - convert all TODO entries to use GitHub issues.
// TODO(aryann) - follow Google Style Guide wrt to default arguments and virtual
//     functions.
// NOLINTNEXTLINE(google-default-arguments)
StatusOr<std::vector<ReadStream>> ConnectionImpl::ParallelRead(
    std::string const& parent_project_id, std::string const& table,
    std::vector<std::string> const& columns) {
  auto response = NewReadSession(parent_project_id, table, columns);
  if (!response.ok()) {
    return response.status();
  }

  std::vector<ReadStream> result;
  for (bigquerystorage_proto::Stream const& stream :
       response.value().streams()) {
    result.push_back(MakeReadStream(stream.name()));
  }
  return result;
}

StatusOr<bigquerystorage_proto::ReadSession> ConnectionImpl::NewReadSession(
    std::string const& parent_project_id, std::string const& table,
    std::vector<std::string> const& columns) {
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
  for (std::string const& column : columns) {
    request.mutable_read_options()->add_selected_fields(column);
  }

  return read_stub_->CreateReadSession(request);
}

std::shared_ptr<ConnectionImpl> MakeConnection(
    std::shared_ptr<StorageStub> read_stub) {
  return std::shared_ptr<ConnectionImpl>(
      new ConnectionImpl(std::move(read_stub)));
}

}  // namespace internal
}  // namespace BIGQUERY_CLIENT_NS
}  // namespace bigquery
}  // namespace cloud
}  // namespace google
