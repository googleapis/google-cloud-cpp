# BigQuery API C++ Client Library

:construction:

This directory contains an idiomatic C++ client library for the
[BigQuery API][cloud-service-docs].

A data platform for customers to create, manage, share and query data.

While this library is **GA**, please note Google Cloud C++ client libraries do
 **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/bigquerycontrol/v2/job_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  auto const project_id = argv[1];

  namespace bigquerycontrol = ::google::cloud::bigquerycontrol_v2;
  namespace bigquery_v2_proto = ::google::cloud::bigquery::v2;
  auto client = bigquerycontrol::JobServiceClient(
      bigquerycontrol::MakeJobServiceConnectionRest());

  bigquery_v2_proto::ListJobsRequest list_request;
  list_request.set_project_id(project_id);

  for (auto r : client.ListJobs(list_request)) {
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

- Official documentation about the [BigQuery API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/bigquery/docs
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/bigquerycontrol/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/bigquerycontrol
