# Cloud Dataproc API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Cloud Dataproc API][cloud-service-docs], a managed Apache Spark and Apache
Hadoop service that lets you take advantage of open source data tools for batch
processing, querying, streaming, and machine learning. This library allows you
to *manage* Cloud Dataproc resources, but it does not provide APIs to run C++
applications in Cloud Dataproc.

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
#include "google/cloud/dataproc/v1/cluster_controller_client.h"
#include "google/cloud/common_options.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id region\n";
    return 1;
  }
  std::string const project_id = argv[1];
  std::string const region = argv[2];

  namespace dataproc = ::google::cloud::dataproc_v1;

  auto client = dataproc::ClusterControllerClient(
      dataproc::MakeClusterControllerConnection(region == "global" ? ""
                                                                   : region));

  for (auto c : client.ListClusters(project_id, region)) {
    if (!c) throw std::move(c).status();
    std::cout << c->cluster_name() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Cloud Dataproc API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/dataproc
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/dataproc/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/dataproc
