# Policy Simulator API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Policy Simulator API][cloud-service-docs].

Policy Simulator is a collection of endpoints for creating, running, and viewing
a `Replay`. A `Replay` is a type of simulation that lets you see how your
members' access to resources might change if you changed your IAM policy.

During a `Replay`, Policy Simulator re-evaluates, or replays, past access
attempts under both the current policy and your proposed policy, and compares
those results to determine how your members' access might change under the
proposed policy.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/policysimulator/v1/simulator_client.h"
#include "google/cloud/location.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr
        << "Usage: " << argv[0] << " project-id resource-name\n"
        << "See https://cloud.google.com/iam/docs/full-resource-names for "
           "examples of fully qualified resource names.\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], "global");
  auto const resource_name = std::string{argv[2]};

  namespace policysimulator = ::google::cloud::policysimulator_v1;
  auto client = policysimulator::SimulatorClient(
      policysimulator::MakeSimulatorConnection());

  google::cloud::policysimulator::v1::Replay r;
  auto& overlay = *r.mutable_config()->mutable_policy_overlay();
  overlay[resource_name] = [] {
    google::iam::v1::Policy p;
    auto& binding = *p.add_bindings();
    binding.set_role("storage.buckets.get");
    binding.add_members("user@example.com");
    return p;
  }();

  auto replay = client.CreateReplay(location.FullName(), r).get();
  if (!replay) throw std::move(replay).status();
  std::cout << replay->DebugString() << "\n";

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Policy Simulator API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/policy-intelligence/docs/iam-simulator-overview
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/policysimulator/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/policysimulator
