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

#include "google/cloud/bigquery/client.h"

namespace {

using google::cloud::StatusOr;
using google::cloud::bigquery::Client;
using google::cloud::bigquery::ConnectionOptions;
using google::cloud::bigquery::DeserializeReadStream;
using google::cloud::bigquery::MakeConnection;
using google::cloud::bigquery::ReadResult;
using google::cloud::bigquery::ReadStream;
using google::cloud::bigquery::Row;
using google::cloud::bigquery::SerializeReadStream;

// The following are some temporary examples of how I envision the Read()
// functions will be used. Once we settle on the design and implementation,
// we'll restructure these samples, so users can actually run them.

void SimpleRead() {
  ConnectionOptions options;
  Client client(MakeConnection(options));
  ReadResult<Row> result = client.Read(
      "my-parent-project", "bigquery-public-data:samples.shakespeare",
      /* columns = */ {"c1", "c2", "c3"});
  for (StatusOr<Row> const& row : result.Rows()) {
    if (row.ok()) {
      // Do something with row.value();
    }
  }
}

void ParallelRead() {
  // From coordinating job:
  ConnectionOptions options;
  Client client(MakeConnection(options));
  StatusOr<std::vector<ReadStream<Row>>> read_session = client.ParallelRead(
      "my-parent-project", "bigquery-public-data:samples.shakespeare",
      /* columns = */ {"c1", "c2", "c3"});
  if (!read_session.ok()) {
    // Handle error;
  }

  for (ReadStream<Row> const& stream : read_session.value()) {
    std::string bits = SerializeReadStream(stream).value();
    // Send bits to worker job.
  }

  // From a worker job:
  std::string bits;  // Sent by coordinating job.
  ReadStream<Row> stream = DeserializeReadStream<Row>(bits).value();
  ReadResult<Row> result = client.Read(stream);
  for (StatusOr<Row> const& row : result.Rows()) {
    if (row.ok()) {
      // Do something with row.value();
    }
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "You must provide a project ID as a positional argument.\n";
    return EXIT_FAILURE;
  }
  std::string project_id = argv[1];

  google::cloud::bigquery::ConnectionOptions options;
  google::cloud::bigquery::Client client(
      google::cloud::bigquery::MakeConnection(options));
  google::cloud::StatusOr<std::string> res = client.CreateSession(
      project_id, "bigquery-public-data:samples.shakespeare");

  if (res.ok()) {
    std::cout << "Session name: " << res.value() << "\n";
    return EXIT_SUCCESS;
  } else {
    std::cerr << "Session creation failed with error: " << res.status() << "\n";
    return EXIT_FAILURE;
  }
}
