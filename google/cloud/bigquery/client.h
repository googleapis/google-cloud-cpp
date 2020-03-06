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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_CLIENT_H

#include "google/cloud/bigquery/connection.h"
#include "google/cloud/bigquery/connection_options.h"
#include "google/cloud/bigquery/read_result.h"
#include "google/cloud/bigquery/read_stream.h"
#include "google/cloud/bigquery/row.h"
#include "google/cloud/bigquery/version.h"
#include "google/cloud/status_or.h"
#include <memory>

namespace google {
namespace cloud {
namespace bigquery {
inline namespace BIGQUERY_CLIENT_NS {
class Client {
 public:
  explicit Client(std::shared_ptr<Connection> conn) : conn_(std::move(conn)) {}

  Client() = delete;

  // Client is copyable and movable.
  Client(Client const&) = default;
  Client& operator=(Client const&) = default;
  Client(Client&&) = default;
  Client& operator=(Client&&) = default;

  friend bool operator==(Client const& a, Client const& b) {
    return a.conn_ == b.conn_;
  }

  friend bool operator!=(Client const& a, Client const& b) { return !(a == b); }

  // Reads the given table.
  //
  // The read is performed on behalf of `parent_project_id`.
  //
  // `table` must be in the form `PROJECT_ID:DATASET_ID.TABLE_ID`.
  //
  // There are no row ordering guarantees.
  ReadResult Read(std::string const& parent_project_id,
                  std::string const& table,
                  std::vector<std::string> const& columns = {});

  // Performs a read using a `ReadStream` returned by
  // `bigquery::Client::ParallelRead()`. See the documentation of
  // `ParallelRead()` for more information.
  ReadResult Read(ReadStream const& read_stream);

  // Creates one or more `ReadStream`s that can be used to read data from a
  // table in parallel.
  //
  // There are no row ordering guarantees. There are also no guarantees about
  // which rows are assigned to which `ReadStream`s.
  //
  // Additionally, multiple calls to this function with the same inputs are not
  // guaranteed to produce the same distribution or order of rows.
  //
  // After 24 hours, all `ReadStreams` created will stop working.
  StatusOr<std::vector<ReadStream>> ParallelRead(
      std::string const& parent_project_id, std::string const& table,
      std::vector<std::string> const& columns = {});

 private:
  std::shared_ptr<Connection> conn_;
};

std::shared_ptr<Connection> MakeConnection(ConnectionOptions const& options);

}  // namespace BIGQUERY_CLIENT_NS
}  // namespace bigquery
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_CLIENT_H
