# Privileged Access Manager API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Privileged Access Manager API][cloud-service-docs].

Privileged Access Manager (PAM) helps you on your journey towards least
privilege and helps mitigate risks tied to privileged access misuse or abuse.
PAM allows you to shift from always-on standing privileges towards on-demand
access with just-in-time, time-bound, and approval-based access elevations. PAM
allows IAM administrators to create entitlements that can grant just-in-time,
temporary access to any resource scope. Requesters can explore eligible
entitlements and request the access needed for their task. Approvers are
notified when approvals await their decision. Streamlined workflows facilitated
by using PAM can support various use cases, including emergency access for
incident responders, time-boxed access for developers for critical deployment or
maintenance, temporary access for operators for data ingestion and audits, JIT
access to service accounts for automated tasks, and more.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/privilegedaccessmanager/v1/privileged_access_manager_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], argv[2]);

  namespace pam = ::google::cloud::privilegedaccessmanager_v1;
  auto client = pam::PrivilegedAccessManagerClient(
      pam::MakePrivilegedAccessManagerConnection());

  for (auto r : client.ListEntitlements(location.FullName())) {
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

- Official documentation about the
  [Privileged Access Manager API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/iam/docs/pam-overview
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/privilegedaccessmanager/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/privilegedaccessmanager
