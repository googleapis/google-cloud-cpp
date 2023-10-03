# Advisory Notifications API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Advisory Notifications API][cloud-service-docs], a service to manage Security
and Privacy Notifications.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/advisorynotifications/v1/advisory_notifications_client.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " organization-id location-id\n";
    return 1;
  }

  namespace advisorynotifications = ::google::cloud::advisorynotifications_v1;
  auto client = advisorynotifications::AdvisoryNotificationsServiceClient(
      advisorynotifications::MakeAdvisoryNotificationsServiceConnection());
  auto const parent =
      std::string{"organizations/"} + argv[1] + "/locations/" + argv[2];
  for (auto n : client.ListNotifications(parent)) {
    if (!n) throw std::move(n).status();
    std::cout << n->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the
  [Advisory Notifications API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/advisory-notifications
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/advisorynotifications/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/advisorynotifications
