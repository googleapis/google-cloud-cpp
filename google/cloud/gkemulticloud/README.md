# Anthos Multi-Cloud API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Anthos Multi-Cloud API][cloud-service-docs]. This API provides a way to manage
Kubernetes clusters that run on AWS and Azure infrastructure. Combined with
Connect, you can manage Kubernetes clusters on Google Cloud, AWS, and Azure from
the Google Cloud Console. When you create a cluster with Anthos Multi-Cloud,
Google creates the resources needed and brings up a cluster on your behalf. You
can deploy workloads with the Anthos Multi-Cloud API or the gcloud and kubectl
command-line tools.

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
#include "google/cloud/gkemulticloud/v1/attached_clusters_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id region-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], argv[2]);

  namespace gkemulticloud = ::google::cloud::gkemulticloud_v1;
  auto client = gkemulticloud::AttachedClustersClient(
      gkemulticloud::MakeAttachedClustersConnection(location.location_id()));

  for (auto ac : client.ListAttachedClusters(location.FullName())) {
    if (!ac) throw std::move(ac).status();
    std::cout << ac->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Anthos Multi-Cloud API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/anthos/clusters/docs/multi-cloud
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/gkemulticloud/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/gkemulticloud
