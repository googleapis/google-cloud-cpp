# Google Cloud Bigtable C++ Client Library

This directory contains an idiomatic C++ client library for interacting with
[Google Cloud Bigtable](https://cloud.google.com/bigtable/), which is Google's
NoSQL Big Data database service. It's the same database that powers many core
Google services, including Search, Analytics, Maps, and Gmail.

While this library is **GA**, please note that the Google Cloud C++ client libraries do **not** follow
[Semantic Versioning](http://semver.org/).

## Supported Platforms

- Windows, macOS, Linux
- C++14 (and higher) compilers (we test with GCC >= 7.3, Clang >= 6.0, and
  MSVC >= 2017)
- Environments with or without exceptions
- Bazel (>= 4.0) and CMake (>= 3.5) builds

## Documentation

- Official documentation about the [Cloud Bigtable][cloud-bigtable-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this client library
- Detailed header comments in our [public `.h`][source-link] files

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/bigtable/table.h"

int main(int argc, char* argv[]) try {
  if (argc != 4) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(cmd).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <project_id> <instance_id> <table_id>\n";
    return 1;
  }

  std::string const project_id = argv[1];
  std::string const instance_id = argv[2];
  std::string const table_id = argv[3];

  // Create a namespace alias to make the code easier to read.
  namespace cbt = ::google::cloud::bigtable;

  cbt::Table table(cbt::MakeDataConnection(),
                   cbt::TableResource(project_id, instance_id, table_id));

  std::string row_key = "r1";
  std::string column_family = "cf1";

  std::cout << "Getting a single row by row key:" << std::flush;
  google::cloud::StatusOr<std::pair<bool, cbt::Row>> result =
      table.ReadRow(row_key, cbt::Filter::FamilyRegex(column_family));
  if (!result) throw std::runtime_error(result.status().message());
  if (!result->first) {
    std::cout << "Cannot find row " << row_key << " in the table: " << table_id
              << "\n";
    return 0;
  }

  cbt::Cell const& cell = result->second.cells().front();
  std::cout << cell.family_name() << ":" << cell.column_qualifier() << "    @ "
            << cell.timestamp().count() << "us\n"
            << '"' << cell.value() << '"' << "\n";

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

- Packaging maintainers or developers who prefer to install the library in a
  fixed directory (such as `/usr/local` or `/opt`) should consult the
  [packaging guide](/doc/packaging.md).
- Developers wanting to use this client library as part of a larger CMake or
  Bazel project should consult the aforementioned
  [quickstart](quickstart/README.md).
- Developers wanting to compile the library just to run some of the examples or
  test should consult the
  [building and installing](/README.md#building-and-installing) section of the
  top-level README file.
- Contributors and developers to `google-cloud-cpp` should consult the guide to
  [setup a development workstation][howto-setup-dev-workstation].

## Contributing changes

See [`CONTRIBUTING.md`](/CONTRIBUTING.md) for details on how to
contribute to this project, including how to build and test your changes
as well as how to properly format your code.

## Licensing

Apache 2.0; see [`LICENSE`](/LICENSE) for details.

[cloud-bigtable-docs]: https://cloud.google.com/bigtable/docs/
[doxygen-link]: https://googleapis.dev/cpp/google-cloud-bigtable/latest/
[howto-setup-dev-workstation]: /doc/contributor/howto-guide-setup-development-workstation.md
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/bigtable
