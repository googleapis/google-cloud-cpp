# Google Cloud BigQuery C++ Client Library

This directory contains an idiomatic C++ client library for interacting with
[Cloud BigQuery](https://cloud.google.com/bigquery/).

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

For detailed instructions on how to build and install this library, see the
top-level [README](/README.md#building-and-installing).

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/bigquery/storage/v1/bigquery_read_client.h"
#include <iostream>

namespace {
void ProcessRowsInAvroFormat(
    ::google::cloud::bigquery::storage::v1::AvroSchema const&,
    ::google::cloud::bigquery::storage::v1::AvroRows const&) {
  // Code to deserialize avro rows should be added here.
}
}  // namespace

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <project-id> <table-name>\n";
    return 1;
  }

  // project_name should be in the format "projects/<your-gcp-project>"
  std::string const project_name = "projects/" + std::string(argv[1]);
  // table_name should be in the format:
  // "projects/<project-table-resides-in>/datasets/<dataset-table_resides-in>/tables/<table
  // name>" The project values in project_name and table_name do not have to be
  // identical.
  std::string const table_name = argv[2];

  // Create a namespace alias to make the code easier to read.
  namespace bigquery_storage = ::google::cloud::bigquery_storage_v1;
  constexpr int kMaxReadStreams = 1;
  // Create the ReadSession.
  auto client = bigquery_storage::BigQueryReadClient(
      bigquery_storage::MakeBigQueryReadConnection());
  ::google::cloud::bigquery::storage::v1::ReadSession read_session;
  read_session.set_data_format(
      google::cloud::bigquery::storage::v1::DataFormat::AVRO);
  read_session.set_table(table_name);
  auto session =
      client.CreateReadSession(project_name, read_session, kMaxReadStreams);
  if (!session) throw std::move(session).status();

  // Read rows from the ReadSession.
  constexpr int kRowOffset = 0;
  auto read_rows = client.ReadRows(session->streams(0).name(), kRowOffset);

  std::int64_t num_rows = 0;
  for (auto const& row : read_rows) {
    if (row.ok()) {
      num_rows += row->row_count();
      ProcessRowsInAvroFormat(session->avro_schema(), row->avro_rows());
    }
  }

  std::cout << num_rows << " rows read from table: " << table_name << "\n";
  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Cloud BigQuery][cloud-bigquery-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-bigquery-docs]: https://cloud.google.com/bigquery/docs/
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/bigquery/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/bigquery
