# Google Cloud IAM C++ Client Library

This directory contains an idiomatic C++ client library for interacting with
[Cloud IAM](https://cloud.google.com/iam/).

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
#include "google/cloud/iam/admin/v1/iam_client.h"
#include "google/cloud/project.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <project-id>\n";
    return 1;
  }

  // Create a namespace alias to make the code easier to read.
  namespace iam = ::google::cloud::iam_admin_v1;
  iam::IAMClient client(iam::MakeIAMConnection());
  auto const project = google::cloud::Project(argv[1]);
  std::cout << "Service Accounts for project: " << project.project_id() << "\n";
  int count = 0;
  for (auto sa : client.ListServiceAccounts(project.FullName())) {
    if (!sa) throw std::move(sa).status();
    std::cout << sa->name() << "\n";
    ++count;
  }
  if (count == 0) std::cout << "No Service Accounts found.\n";

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Cloud IAM][cloud-iam-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-iam-docs]: https://cloud.google.com/iam/docs/
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/iam/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/iam
