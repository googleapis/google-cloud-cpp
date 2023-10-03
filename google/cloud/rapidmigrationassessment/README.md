# Rapid Migration Assessment API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Rapid Migration Assessment API][cloud-service-docs], a unified migration
platform that helps you accelerate your end-to-end cloud migration journey from
your current on-premises environment to Google Cloud.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/rapidmigrationassessment/v1/rapid_migration_assessment_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], argv[2]);

  namespace rapidmigrationassessment =
      ::google::cloud::rapidmigrationassessment_v1;
  auto client = rapidmigrationassessment::RapidMigrationAssessmentClient(
      rapidmigrationassessment::MakeRapidMigrationAssessmentConnection());

  for (auto c : client.ListCollectors(location.FullName())) {
    if (!c) throw std::move(c).status();
    std::cout << c->DebugString() << "\n";
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
  [Rapid Migration Assessment API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/migration-center/docs
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/rapidmigrationassessment/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/rapidmigrationassessment
