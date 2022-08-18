# Cloud Run Admin API C++ Client Library

This directory contains an idiomatic C++ client library for
[Cloud Run][cloud-service-root], a managed compute platform that lets
you run containers directly on top of Google's scalable infrastructure.

Note that this library only provides tools to **manage** Cloud Run resources. To
actually deploy a C++ function to Cloud Run, see the
[Cloud Run for C++ Hello World][hello-world] example, which uses the
[C++ Functions Framework][functions-framework] library.

[hello-world]: https://github.com/GoogleCloudPlatform/cpp-samples/tree/main/cloud-run-hello-world
[functions-framework]: https://github.com/GoogleCloudPlatform/functions-framework-cpp

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Supported Platforms

* Windows, macOS, Linux
* C++14 (and higher) compilers (we test with GCC >= 7.3, Clang >= 6.0, and
  MSVC >= 2017)
* Environments with or without exceptions
* Bazel (>= 4.0) and CMake (>= 3.5) builds

## Documentation

* Official documentation about the [Cloud Run Admin API][cloud-service-docs] service
* [Reference doxygen documentation][doxygen-link] for each release of this
  client library
* Detailed header comments in our [public `.h`][source-link] files

[cloud-service-root]: https://cloud.google.com/run
[cloud-service-docs]: https://cloud.google.com/run/docs
[doxygen-link]: https://googleapis.dev/cpp/google-cloud-run/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/run

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->
```cc
#include "google/cloud/run/services_client.h"
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  namespace run = ::google::cloud::run;
  auto client = run::ServicesClient(run::MakeServicesConnection());

  auto const parent =
      std::string{"projects/"} + argv[1] + "/locations/" + argv[2];
  for (auto r : client.ListServices(parent)) {
    if (!r) throw std::runtime_error(r.status().message());
    std::cout << r->DebugString() << "\n";
  }

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

## Contributing changes

See [`CONTRIBUTING.md`](/CONTRIBUTING.md) for details on how to
contribute to this project, including how to build and test your changes
as well as how to properly format your code.

## Licensing

Apache 2.0; see [`LICENSE`](/LICENSE) for details.
