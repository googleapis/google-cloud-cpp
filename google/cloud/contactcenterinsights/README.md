# Contact Center AI Insights API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Contact Center AI Insights API][cloud-service-docs], a service that helps users
detect and visualize patterns in their contact center data.

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
#include "google/cloud/contactcenterinsights/v1/contact_center_insights_client.h"
#include "google/cloud/location.h"
#include "google/protobuf/util/time_util.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], argv[2]);

  namespace ccai = ::google::cloud::contactcenterinsights_v1;
  auto client = ccai::ContactCenterInsightsClient(
      ccai::MakeContactCenterInsightsConnection());

  for (auto c : client.ListConversations(location.FullName())) {
    if (!c) throw std::move(c).status();

    using ::google::protobuf::util::TimeUtil;
    std::cout << c->name() << "\n";
    std::cout << "Duration: " << TimeUtil::ToString(c->duration())
              << "; Turns: " << c->turn_count() << "\n\n";
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
  [Contact Center AI Insights API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/contact-center/insights/docs
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/contactcenterinsights/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/contactcenterinsights
