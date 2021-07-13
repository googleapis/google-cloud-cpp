// Copyright 2021 Google LLC
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

#include "google/cloud/bigquery/bigquery_read_client.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/example_driver.h"
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <tuple>
#include <utility>

namespace {

void ExampleStatusOr(std::vector<std::string> const& argv) {
  if (argv.size() != 2) {
    throw google::cloud::testing_util::Usage(
        "example-status-or <project-id> <table-name>");
  }
  //! [example-status-or]
  namespace bigquery = ::google::cloud::bigquery;
  [](std::string const& project_id, std::string const& table_name) {
    int max_stream_count = 1;
    google::cloud::bigquery::storage::v1::ReadSession read_session;
    read_session.set_table(table_name);
    bigquery::BigQueryReadClient client(bigquery::MakeBigQueryReadConnection());
    // The actual type of `session` is
    // google::cloud::StatusOr<google::cloud::bigquery::storage::v1::ReadSession>,
    // but we expect it'll most often be declared with auto like this.
    auto session = client.CreateReadSession("projects/" + project_id,
                                            read_session, max_stream_count);
    if (!session) {
      std::cerr << session.status() << "\n";
      return;
    }

    std::cout << "ReadSession successfully created: " << session->name()
              << ".\n";
  }
  //! [example-status-or]
  (argv.at(0), argv.at(1));
}

void CreateReadSession(std::vector<std::string> const& argv) {
  if (argv.size() != 2) {
    throw google::cloud::testing_util::Usage(
        "create-read-session <project-id> <table-name>");
  }
  //! [bigquery-create-read-session]
  namespace bigquery = ::google::cloud::bigquery;
  [](std::string const& project_id, std::string const& table_name) {
    bigquery::BigQueryReadClient client(bigquery::MakeBigQueryReadConnection());
    int max_stream_count = 1;
    google::cloud::bigquery::storage::v1::ReadSession read_session;
    read_session.set_table(table_name);
    auto session = client.CreateReadSession("projects/" + project_id,
                                            read_session, max_stream_count);
    if (!session) {
      std::cerr << session.status() << "\n";
      return;
    }

    std::cout << "ReadSession successfully created: " << session->name()
              << ".\n";
  }
  //! [bigquery-create-read-session]
  (argv.at(0), argv.at(1));
}

void ReadRows(std::vector<std::string> const& argv) {
  if (argv.size() < 2) {
    throw google::cloud::testing_util::Usage(
        "read-rows <project-id> <table-name> [<row-restriction>]");
  }
  //! [bigquery-read-rows]
  namespace bigquery = ::google::cloud::bigquery;
  [](std::string const& project_id, std::string const& table_name,
     std::string const& row_restriction) {
    bigquery::BigQueryReadClient client(bigquery::MakeBigQueryReadConnection());
    int max_stream_count = 1;
    google::cloud::bigquery::storage::v1::ReadSession read_session;
    read_session.set_table(table_name);
    read_session.set_data_format(
        google::cloud::bigquery::storage::v1::DataFormat::AVRO);
    read_session.mutable_read_options()->set_row_restriction(row_restriction);
    auto session = client.CreateReadSession("projects/" + project_id,
                                            read_session, max_stream_count);
    if (!session) throw std::runtime_error(session.status().message());

    std::int64_t row_count = 0;
    for (auto const& row : client.ReadRows(session->streams(0).name(), 0)) {
      if (!row) throw std::runtime_error(row.status().message());
      row_count += row->row_count();
    }

    std::cout << "ReadRows successfully read " << row_count << "rows from "
              << table_name << ".\n";
  }
  //! [bigquery-read-rows]
  (argv.at(0), argv.at(1), argv.at(2));
}

void SplitReadStream(std::vector<std::string> const& argv) {
  if (argv.size() < 2) {
    throw google::cloud::testing_util::Usage(
        "split-read-stream <project-id> <table-name> [<row-restriction>]");
  }
  //! [bigquery-split-read-stream]
  namespace bigquery = ::google::cloud::bigquery;
  [](std::string const& project_id, std::string const& table_name,
     std::string const& row_restriction) {
    bigquery::BigQueryReadClient client(bigquery::MakeBigQueryReadConnection());
    int max_stream_count = 1;
    google::cloud::bigquery::storage::v1::ReadSession read_session;
    read_session.set_table(table_name);
    read_session.set_data_format(
        google::cloud::bigquery::storage::v1::DataFormat::AVRO);
    read_session.mutable_read_options()->set_row_restriction(row_restriction);
    auto session = client.CreateReadSession("projects/" + project_id,
                                            read_session, max_stream_count);
    if (!session) throw std::runtime_error(session.status().message());

    ::google::cloud::bigquery::storage::v1::SplitReadStreamRequest
        split_request;
    split_request.set_name(session->streams(0).name());
    split_request.set_fraction(0.5);
    auto split_response = client.SplitReadStream(split_request);

    std::int64_t row_count = 0;
    for (auto const& row :
         client.ReadRows(split_response->primary_stream().name(), 0)) {
      if (!row) throw std::runtime_error(row.status().message());
      row_count += row->row_count();
    }
    std::cout << "Successfully read " << row_count
              << "rows from first stream.\n";

    row_count = 0;
    for (auto const& row :
         client.ReadRows(split_response->remainder_stream().name(), 0)) {
      if (!row) throw std::runtime_error(row.status().message());
      row_count += row->row_count();
    }
    std::cout << "Successfully read " << row_count
              << "rows from second stream.\n";
  }
  //! [bigquery-split-read-stream]
  (argv.at(0), argv.at(1), argv.at(2));
}

void AutoRun(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
  });
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
  if (!argv.empty()) throw examples::Usage{"auto"};

  std::string const table_name =
      "projects/bigquery-public-data/datasets/usa_names/tables/"
      "usa_1910_current";

  std::vector<std::string> my_args = {project_id, table_name};
  assert(my_args.size() == 2);
  ExampleStatusOr(my_args);
  CreateReadSession({project_id, table_name});
  ReadRows({project_id, table_name, R"(state = "WA")"});
  SplitReadStream({project_id, table_name, R"(state = "WA")"});

  std::cout << "\nAutoRun done" << std::endl;
}

}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  google::cloud::testing_util::Example example(
      {{"example-status-or", ExampleStatusOr},
       {"create-read-session", CreateReadSession},
       {"read-rows", ReadRows},
       {"split-read-stream", SplitReadStream},
       {"auto", AutoRun}});
  return example.Run(argc, argv);
}
