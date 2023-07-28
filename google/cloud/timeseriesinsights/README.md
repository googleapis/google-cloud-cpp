# Timeseries Insights API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Timeseries Insights API][cloud-service-docs], with this API users can perform
time series spike, trend, and anomaly detection. With a straightforward API and
easy to understand results, the service makes it simple to gather insights from
large amounts of time series data (e.g. monitoring datasets) and integrate these
insights in their applications.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/timeseriesinsights/v1/timeseries_insights_controller_client.h"
#include "google/cloud/project.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  namespace timeseriesinsights = ::google::cloud::timeseriesinsights_v1;
  auto client = timeseriesinsights::TimeseriesInsightsControllerClient(
      timeseriesinsights::MakeTimeseriesInsightsControllerConnection());

  auto const project = google::cloud::Project(argv[1]);
  for (auto r : client.ListDataSets(project.FullName())) {
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

- Official documentation about the [Timeseries Insights API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/timeseries-insights
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/timeseriesinsights/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/timeseriesinsights
