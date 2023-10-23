# Policy Troubleshooter API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Policy Troubleshooter][cloud-service-docs], a service that makes it easier to
understand why a user has access to a resource or doesn't have permission to
call an API.

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
#include "google/cloud/policytroubleshooter/v1/iam_checker_client.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0] << " principal resource-name"
              << " permission\n";
    return 1;
  }

  namespace policytroubleshooter = ::google::cloud::policytroubleshooter_v1;
  auto client = policytroubleshooter::IamCheckerClient(
      policytroubleshooter::MakeIamCheckerConnection());

  google::cloud::policytroubleshooter::v1::TroubleshootIamPolicyRequest request;
  auto& access_tuple = *request.mutable_access_tuple();
  access_tuple.set_principal(argv[1]);
  access_tuple.set_full_resource_name(argv[2]);
  access_tuple.set_permission(argv[3]);
  auto response = client.TroubleshootIamPolicy(request);
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

- Official documentation about the
  [Policy Troubleshooter API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/policy-intelligence/docs/troubleshoot-access
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/policytroubleshooter/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/policytroubleshooter
