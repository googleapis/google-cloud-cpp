// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigquery/storage/v1/bigquery_read_client.h"
#include "google/cloud/bigquery/storage/v1/bigquery_read_options.h"
#include "google/cloud/universe_domain.h"
#include "google/cloud/universe_domain_options.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 4 && argc != 3) {
    std::cerr << "Usage: " << argv[0]
              << " [<project-id> <dataset-id> <table-id> | "
                 "<billing-project-id> <full-table-path>]\n";
    return 1;
  }
  std::string const project_name = std::string{"projects/"} + argv[1];
  std::string const table_path = [&] {
    if (argc == 3) return std::string{argv[2]};
    return project_name + "/datasets/" + argv[2] + "/tables/" + argv[3];
  }();

  // Create a namespace alias to make the code easier to read.
  namespace bigquery_storage = ::google::cloud::bigquery_storage_v1;
  constexpr int kMaxReadStreams = 1;

  auto options =
      google::cloud::AddUniverseDomainOption(google::cloud::ExperimentalTag{});
  if (!options.ok()) throw std::move(options).status();

  // Override retry policy to quickly exit if there's a failure.
  options->set<bigquery_storage::BigQueryReadRetryPolicyOption>(
      std::make_shared<
          bigquery_storage::BigQueryReadLimitedErrorCountRetryPolicy>(3));
  auto client = bigquery_storage::BigQueryReadClient(
      bigquery_storage::MakeBigQueryReadConnection(*options));
  ::google::cloud::bigquery::storage::v1::ReadSession read_session;
  read_session.set_data_format(
      google::cloud::bigquery::storage::v1::DataFormat::AVRO);
  read_session.set_table(table_path);
  auto session =
      client.CreateReadSession(project_name, read_session, kMaxReadStreams);
  if (!session) throw std::move(session).status();

  constexpr int kRowOffset = 0;
  auto read_rows = client.ReadRows(session->streams(0).name(), kRowOffset);

  std::int64_t num_rows = 0;
  for (auto const& row : read_rows) {
    if (row.ok()) {
      num_rows += row->row_count();
    }
  }

  std::cout << num_rows << " rows read from table: " << table_path << "\n";
  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
