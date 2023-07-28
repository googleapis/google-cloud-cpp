# Essential Contacts API C++ Client Library

Many Google Cloud services, such as Cloud Billing, send out notifications to
share important information with Google Cloud users. By default, these
notifications are sent to members with certain Identity and Access Management
(IAM) roles. With the [Essential Contacts API][cloud-service-docs], you can
customize who receives notifications by providing your own list of contacts.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/essentialcontacts/v1/essential_contacts_client.h"
#include "google/cloud/project.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  namespace essentialcontacts = ::google::cloud::essentialcontacts_v1;
  auto client = essentialcontacts::EssentialContactsServiceClient(
      essentialcontacts::MakeEssentialContactsServiceConnection());

  auto const project = google::cloud::Project(argv[1]);
  for (auto r : client.ListContacts(project.FullName())) {
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

## More Information

- Official documentation about the [Essential Contacts API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/essentialcontacts
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/essentialcontacts/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/essentialcontacts
