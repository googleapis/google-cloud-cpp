# Cloud OS Login API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Cloud OS Login API][cloud-service-docs], a service you can use to manage access
to your VM instances using IAM roles.

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
#include "google/cloud/oslogin/v1/os_login_client.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " user\n";
    return 1;
  }

  namespace oslogin = ::google::cloud::oslogin_v1;
  auto client =
      oslogin::OsLoginServiceClient(oslogin::MakeOsLoginServiceConnection());

  auto const name = "users/" + std::string{argv[1]};
  auto lp = client.GetLoginProfile(name);
  if (!lp) throw std::move(lp).status();
  std::cout << lp->DebugString() << "\n";

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Cloud OS Login API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/compute/docs/oslogin
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/oslogin/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/oslogin
