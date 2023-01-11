# Google Cloud Spanner C++ Client Library

This directory contains an idiomatic C++ client library for interacting with
[Google Cloud Spanner](https://cloud.google.com/spanner/), which is a fully
managed, scalable, relational database service for regional and global
application data.

While this library is **GA**, please note that the Google Cloud C++ client libraries do **not** follow
[Semantic Versioning](http://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/spanner/client.h"
#include <iostream>

int main(int argc, char* argv[]) {
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0]
              << " project-id instance-id database-id\n";
    return 1;
  }

  namespace spanner = ::google::cloud::spanner;
  spanner::Client client(
      spanner::MakeConnection(spanner::Database(argv[1], argv[2], argv[3])));

  auto rows =
      client.ExecuteQuery(spanner::SqlStatement("SELECT 'Hello World'"));

  for (auto const& row : spanner::StreamOf<std::tuple<std::string>>(rows)) {
    if (!row) {
      std::cerr << row.status() << "\n";
      return 1;
    }
    std::cout << std::get<0>(*row) << "\n";
  }

  return 0;
}
```

<!-- inject-quickstart-end -->

## Build and Install

- Packaging maintainers or developers who prefer to install the library in a
  fixed directory (such as `/usr/local` or `/opt`) should consult the
  [packaging guide](/doc/packaging.md).
- Developers who prefer using a package manager such as
  [vcpkg](https://vcpkg.io), [Conda](https://conda.io),
  or [Conan](https://conan.io) should follow the instructions for their package
  manager.
- Developers wanting to use the libraries as part of a larger CMake or Bazel
  project should consult the [quickstart guides](#quickstart) for the library
  or libraries they want to use.
- Developers wanting to compile the library just to run some examples or
  tests should read the project's top-level
  [README](/README.md#building-and-installing).
- Contributors and developers to `google-cloud-cpp` should consult the guide to
  [set up a development workstation][howto-setup-dev-workstation].

## More Information

- Official documentation about the [Cloud Spanner][cloud-spanner-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-spanner-docs]: https://cloud.google.com/spanner/docs/
[doxygen-link]: https://googleapis.dev/cpp/google-cloud-spanner/latest/
[howto-setup-dev-workstation]: /doc/contributor/howto-guide-setup-development-workstation.md
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/spanner
