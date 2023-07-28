# Cloud Asset API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Cloud Asset API][cloud-service-docs], a metadata inventory service that allows
you to view, monitor, and analyze all your GCP and Anthos assets across projects
and services.

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
#include "google/cloud/asset/v1/asset_client.h"
#include "google/cloud/project.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  namespace asset = ::google::cloud::asset_v1;
  auto client = asset::AssetServiceClient(asset::MakeAssetServiceConnection());

  auto const project = google::cloud::Project(argv[1]);
  google::cloud::asset::v1::ListAssetsRequest request;
  request.set_parent(project.FullName());
  request.add_asset_types("storage.googleapis.com/Bucket");
  for (auto a : client.ListAssets(request)) {
    if (!a) throw std::move(a).status();
    std::cout << a->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Cloud Asset API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/asset-inventory
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/asset/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/asset
