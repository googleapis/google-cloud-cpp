# Anthos Multi-Cloud API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Anthos Multi-Cloud API][cloud-service-docs]. This API provides a way to manage
Kubernetes clusters that run on AWS and Azure infrastructure. Combined with
Connect, you can manage Kubernetes clusters on Google Cloud, AWS, and Azure
from the Google Cloud Console.  When you create a cluster with Anthos
Multi-Cloud, Google creates the resources needed and brings up a cluster on
your behalf. You can deploy workloads with the Anthos Multi-Cloud API or the
gcloud and kubectl command-line tools.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Supported Platforms

- Windows, macOS, Linux
- C++14 (and higher) compilers (we test with GCC >= 7.3, Clang >= 6.0, and
  MSVC >= 2017)
- Environments with or without exceptions
- Bazel (>= 4.0) and CMake (>= 3.5) builds

## Documentation

- Official documentation about the [Anthos Multi-Cloud API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/gkemulticloud/v1/attached_clusters_client.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id region-id\n";
    return 1;
  }

  namespace gkemulticloud = ::google::cloud::gkemulticloud_v1;
  auto const region = std::string{argv[2]};
  auto client = gkemulticloud::AttachedClustersClient(
      gkemulticloud::MakeAttachedClustersConnection(region));

  auto const parent =
      std::string{"projects/"} + argv[1] + "/locations/" + region;
  for (auto r : client.ListAttachedClusters(parent)) {
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

- Packaging maintainers or developers who prefer to install the library in a
  fixed directory (such as `/usr/local` or `/opt`) should consult the
  [packaging guide](/doc/packaging.md).

* Developers that prefer using a package manager such as
  [vcpkg](https://vcpkg.io), [Conda](https://conda.io),
  or [Conan](https://conan.io) should follow the instructions for their package
  manager.

- Developers wanting to use the libraries as part of a larger CMake or Bazel
  project should consult the [quickstart guides](#quickstart) for the library
  or libraries they want to use.
- Developers wanting to compile the library just to run some examples or
  tests should read the current document.
- Contributors and developers to `google-cloud-cpp` should consult the guide to
  [set up a development workstation][howto-setup-dev-workstation].

## Contributing changes

See [`CONTRIBUTING.md`](/CONTRIBUTING.md) for details on how to
contribute to this project, including how to build and test your changes
as well as how to properly format your code.

## Licensing

Apache 2.0; see [`LICENSE`](/LICENSE) for details.

[cloud-service-docs]: https://cloud.google.com/anthos/clusters/docs/multi-cloud
[doxygen-link]: https://googleapis.dev/cpp/google-cloud-gkemulticloud/latest/
[howto-setup-dev-workstation]: /doc/contributor/howto-guide-setup-development-workstation.md
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/gkemulticloud
