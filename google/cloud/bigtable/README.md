# Google Cloud Bigtable C++ Client Library

This directory contains an idiomatic C++ client library for interacting with
[Google Cloud Bigtable](https://cloud.google.com/bigtable/), which is Google's
NoSQL Big Data database service. It's the same database that powers many core
Google services, including Search, Analytics, Maps, and Gmail.

While this library is **GA**, please note that the Google Cloud C++ client libraries do **not** follow
[Semantic Versioning](http://semver.org/).

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
  if (!result) throw std::move(result).status();
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
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## Build and Install

- Packaging maintainers or developers who prefer to install the library in a
  fixed directory (such as `/usr/local` or `/opt`) should consult the
  [packaging guide](/doc/packaging.md).
- Developers who prefer using a package manager such as
  [vcpkg](https://vcpkg.io), or [Conda](https://conda.io), should follow the
  instructions for their package manager.
- Developers wanting to use the libraries as part of a larger CMake or Bazel
  project should consult the [quickstart guides](#quickstart) for the library
  or libraries they want to use.
- Developers wanting to compile the library just to run some examples or
  tests should read the project's top-level
  [README](/README.md#building-and-installing).
- Contributors and developers to `google-cloud-cpp` should consult the guide to
  [set up a development workstation][howto-setup-dev-workstation].

## More Information

- Official documentation about the [Cloud Bigtable][cloud-bigtable-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this client library
- Detailed header comments in our \[public `.h`\]\[source-link\] files

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

For detailed instructions on how to build and install this library, see the
top-level [README](/README.md#building-and-installing).

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
  if (!result) throw std::move(result).status();
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
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

[cloud-bigtable-docs]: https://cloud.google.com/bigtable/docs/
[doxygen-link]: https://googleapis.dev/cpp/google-cloud-bigtable/latest/
[howto-setup-dev-workstation]: /doc/contributor/howto-guide-setup-development-workstation.md
