# Web Risk API C++ Client Library

This directory contains an idiomatic C++ client library for
[Web Risk][cloud-service-docs], a service to detect malicious URLs on your
website and in client applications.

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
#include "google/cloud/webrisk/v1/web_risk_client.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc > 2) {
    std::cerr << "Usage: " << argv[0]
              << " [uri (default https://www.google.com)]\n";
    return 1;
  }

  namespace webrisk = ::google::cloud::webrisk_v1;
  auto client =
      webrisk::WebRiskServiceClient(webrisk::MakeWebRiskServiceConnection());

  auto const uri = std::string{argc == 2 ? argv[1] : "https://www.google.com/"};
  auto const threat_types = {google::cloud::webrisk::v1::MALWARE,
                             google::cloud::webrisk::v1::UNWANTED_SOFTWARE};
  auto response = client.SearchUris("https://www.google.com/", threat_types);
  if (!response) throw std::move(response).status();
  std::cout << response->DebugString() << "\n";

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Web Risk API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/web-risk
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/webrisk/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/webrisk
