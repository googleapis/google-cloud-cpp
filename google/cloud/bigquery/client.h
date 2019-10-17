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

#ifndef BIGQUERY_CLIENT_H_
#define BIGQUERY_CLIENT_H_

#include <memory>

#include "google/cloud/bigquery/connection.h"
#include "google/cloud/bigquery/connection_options.h"
#include "google/cloud/bigquery/version.h"
#include "google/cloud/status_or.h"

namespace google {
namespace cloud {
namespace bigquery {
inline namespace BIGQUERY_CLIENT_NS {

// TODO(aryann): Move all of the classes defined here except Client to their own
// files.

// TODO(aryann): Add an implementation for a row. We must support schemas that
// are known at compile-time as well as those that are known at run-time.
class Row {};

template <typename RowType>
class RowSet {
 public:
  using value_type = StatusOr<RowType>;
  class iterator {
   public:
    using iterator_category = std::input_iterator_tag;
    using value_type = RowSet::value_type;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

    reference operator*() { return curr_; }
    pointer operator->() { return &curr_; }

    iterator& operator++() { return *this; }

    iterator operator++(int) { return {}; }

    friend bool operator==(iterator const& a, iterator const& b) { return {}; }

    friend bool operator!=(iterator const& a, iterator const& b) {
      return !(a == b);
    }

   private:
    StatusOr<RowType> curr_;
  };

  iterator begin() { return {}; }

  iterator end() { return {}; }
};

// Represents the result of a read operation.
//
// Note that at most one pass can be made over the data returned from a
// `ReadResult`.
template <typename RowType>
class ReadResult {
 public:
  // Returns a `RowSet` which can be used to iterate through the rows that are
  // presented by this object.
  RowSet<RowType> Rows() { return {}; }

  // Returns a zero-based index of the last row returned by the `Rows()`
  // iterator. If no rows have been read yet, returns -1.
  int CurrentOffset() { return {}; }

  // Returns a value between 0 and 1, inclusive, that indicates the progress in
  // the result set based on the number of rows the server has processed.
  //
  // Note that if this ReadResult was created through
  // `bigquery::Client::ParallelRead()` or if a row filter was provided, then
  // the returned value will not necessarily equal to the current offset divided
  // by the number of rows in the `ReadResult`:
  //
  //   * In the case of a parallel read, data are assigned to
  //     `bigquery::ReadStream`s lazily by the server. The server does not know
  //     the total number of rows that will be assigned to the stream ahead of
  //     time, so it uses a denominator that is guaranteed to never exceed the
  //     maximum number of rows that are allowed to be assigned.
  //
  //   * In the presence of a row filter, the denominator is not known until all
  //     rows are read because some rows may be excluded. As such, the server
  //     uses an estimate for the number of pre-filtering rows.
  //
  double FractionConsumed() { return {}; }
};

template <typename RowType>
class ReadStream {};

// Serializes an instance of `ReadStream` for transmission to another process.
template <typename RowType>
StatusOr<std::string> SerializeReadStream(
    ReadStream<RowType> const& read_stream) {
  return {};
}

// Deserializes the provided string to a `ReadStream`, if able.
template <typename RowType>
StatusOr<ReadStream<RowType>> DeserializeReadStream(
    std::string serialized_read_stream) {
  return {};
}

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

  // Creates a new read session and returns its name if successful.
  //
  // This function is just a proof of concept to ensure we can send
  // requests to the server.
  StatusOr<std::string> CreateSession(std::string parent_project_id,
                                      std::string table);

  // Reads the given table.
  //
  // The read is performed on behalf of `parent_project_id`.
  //
  // `table` must be in the form `PROJECT_ID:DATASET_ID.TABLE_ID`.
  //
  // There are no row ordering guarantees.
  ReadResult<Row> Read(std::string parent_project_id, std::string table,
                       std::vector<std::string> columns = {});

  // Performs a read using a `ReadStream` returned by
  // `bigquery::Client::ParallelRead()`. See the documentation of
  // `ParallelRead()` for more information.
  ReadResult<Row> Read(ReadStream<Row> const& read_stream);

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
  StatusOr<std::vector<ReadStream<Row>>> ParallelRead(
      std::string parent_project_id, std::string table,
      std::vector<std::string> columns = {});

 private:
  std::shared_ptr<Connection> conn_;
};

std::shared_ptr<Connection> MakeConnection(ConnectionOptions const& options);

}  // namespace BIGQUERY_CLIENT_NS
}  // namespace bigquery
}  // namespace cloud
}  // namespace google

#endif  // BIGQUERY_CLIENT_H_
