# Cloud Data Loss Prevention (DLP) API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Cloud Data Loss Prevention (DLP) API][cloud-service-docs], a service that
provides methods for detection, risk analysis, and de-identification of
privacy-sensitive fragments in text, images, and Google Cloud Platform storage
repositories.

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
#include "google/cloud/dlp/v2/dlp_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], argv[2]);

  namespace dlp = ::google::cloud::dlp_v2;
  auto client = dlp::DlpServiceClient(dlp::MakeDlpServiceConnection());

  for (auto sit : client.ListStoredInfoTypes(location.FullName())) {
    if (!sit) throw std::move(sit).status();
    std::cout << sit->DebugString() << "\n";
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
  [Cloud Data Loss Prevention (DLP) API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/dlp/docs/
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/dlp/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/dlp
