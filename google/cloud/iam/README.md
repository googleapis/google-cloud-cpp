# Google Cloud IAM C++ Client Library

This directory contains an idiomatic C++ client library for interacting with
[Cloud IAM](https://cloud.google.com/iam/).

Please note that the Google Cloud C++ client libraries do **not** follow
[Semantic Versioning](https://semver.org/).

## Supported Platforms

* Windows, macOS, Linux
* C++11 (and higher) compilers (we test with GCC >= 5.4, Clang >= 6.0, and
  MSVC >= 2017)
* Environments with or without exceptions
* Bazel and CMake builds

## Documentation

* Official documentation about the [Cloud IAM][cloud-iam-docs] service
* [Reference doxygen documentation][doxygen-link] for each release of this client library
* Detailed header comments in our [public `.h`][source-link] files

[doxygen-link]: https://googleapis.dev/cpp/google-cloud-iam/latest/
[cloud-iam-docs]: https://cloud.google.com/iam/docs/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/iam

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->
```cc
#include "google/cloud/iam/iam_client.h"
#include "google/cloud/project.h"
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <project-id>\n";
    return 1;
  }

  // Create a namespace alias to make the code easier to read.
  namespace iam = ::google::cloud::iam;
  iam::IAMClient client(iam::MakeIAMConnection());
  auto const project = google::cloud::Project(argv[1]);
  std::cout << "Service Accounts for project: " << project.project_id() << "\n";
  int count = 0;
  for (auto const& sa : client.ListServiceAccounts(project.FullName())) {
    if (!sa) throw std::runtime_error(sa.status().message());
    std::cout << sa->name() << "\n";
    ++count;
  }
  if (count == 0) std::cout << "No Service Accounts found.\n";

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << "\n";
  return 1;
}
```
<!-- inject-quickstart-end -->

* Packaging maintainers or developers who prefer to install the library in a
  fixed directory (such as `/usr/local` or `/opt`) should consult the
  [packaging guide](/doc/packaging.md).
* Developers wanting to use the libraries as part of a larger CMake or Bazel
  project should consult the [quickstart guides](#quickstart) for the library
  or libraries they want to use.
* Developers wanting to compile the library just to run some of the examples or
  tests should read the current document.
* Contributors and developers to `google-cloud-cpp` should consult the guide to
  [setup a development workstation][howto-setup-dev-workstation].

[howto-setup-dev-workstation]: /doc/contributor/howto-guide-setup-development-workstation.md

## Installation

Please consult the [packaging guide](../../../doc/packaging.md) for detailed
instructions to install the Google Cloud C++ client libraries.
If your project uses [CMake](https://cmake.org) or [Bazel](https://bazel.build)
check the [quickstart](quickstart/README.md) example for instructions to use
this library in your project.

## Contributing changes

See [`CONTRIBUTING.md`](../../../CONTRIBUTING.md) for details on how to
contribute to this project, including how to build and test your changes
as well as how to properly format your code.

## Licensing

Apache 2.0; see [`LICENSE`](../../../LICENSE) for details.
