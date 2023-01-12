# Stackdriver Debugger API C++ Client Library

This directory contains an idiomatic C++ client library for the [Stackdriver
Debugger API][cloud-service-docs], a service to examine the call stack and
variables of a running application without stopping or slowing it down.

:warning: The [Stackdriver Debugger API][cloud-service-docs] has been
[deprecated].  The library will continue to be supported until the service is
shutdown, at which time we will remove the library.

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

For detailed instructions on how to build and install this library, see the
top-level [README](/README.md#building-and-installing).

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/debugger/debugger2_client.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  namespace debugger = ::google::cloud::debugger;
  auto client = debugger::Debugger2Client(debugger::MakeDebugger2Connection());
  auto response = client.ListDebuggees(argv[1], "quickstart");
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

- Official documentation about the [Stackdriver Debugger API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/debugger
[deprecated]: https://cloud.google.com/debugger/docs/deprecations
[doxygen-link]: https://googleapis.dev/cpp/google-cloud-debugger/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/debugger
