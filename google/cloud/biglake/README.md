# Lakehouse API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Lakehouse API][cloud-service-docs].

The Lakehouse API (formerly BigLake API) provides access to a serverless, fully
managed, and highly available metastore that provides a single source of truth
for your data lakehouse. It lets multiple engines—including Apache Spark, Google
Managed Spark, Apache Flink, Trino and BigQuery—share tables and metadata for
key open formats (Apache Iceberg, Apache Hive), and query the same copy of data.
Plus, through the Lakehouse runtime catalog federation seamlessly unite your
lakehouse ecosystem, letting Iceberg compatible engines on Google Cloud
(BigQuery, Google Managed Spark) discover and analyze enterprise data across
Snowflake, Databricks, and AWS Glue.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/biglake/hive/v1/hive_metastore_client.h"
#include "google/cloud/project.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  auto const project = google::cloud::Project(argv[1]);

  namespace biglake = ::google::cloud::biglake_hive_v1;
  auto client = biglake::HiveMetastoreServiceClient(
      biglake::MakeHiveMetastoreServiceConnection());

  for (auto r : client.ListHiveCatalogs(project.FullName())) {
    if (!r) throw std::move(r).status();
    std::cout << r->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Lakehouse API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/bigquery/docs/iceberg-tables#create-using-biglake-metastore
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/biglake/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/biglake
