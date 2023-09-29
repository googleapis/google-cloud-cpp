# Google Cloud C++ OpenTelemetry Exporters

This directory contains components to export [traces] captured with
[OpenTelemetry] to [Cloud Trace]. Traces are a form of telemetry where calls
across multiple distributed systems can be collected in a centralized view,
showing the sequence of calls and their latency. Cloud Trace can provide such
views of your system calls.

The C++ client libraries are already instrumented to collect traces. You can
enable the collection and export of traces with minimal changes to your
application. It is not necessary to instrument your application too.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started exporting traces from the C++ client libraries. The following
program is used in this quickstart.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/opentelemetry/configure_basic_tracing.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/opentelemetry_options.h"
#include <iostream>

int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <bucket-name> <project-id>\n";
    return 1;
  }
  std::string const bucket_name = argv[1];
  std::string const project_id = argv[2];

  // Create aliases to make the code easier to read.
  namespace gc = ::google::cloud;
  namespace gcs = ::google::cloud::storage;

  // Instantiate a basic tracing configuration which exports traces to Cloud
  // Trace. By default, spans are sent in batches and always sampled.
  auto project = gc::Project(project_id);
  auto configuration = gc::otel::ConfigureBasicTracing(project);

  // Create a client with OpenTelemetry tracing enabled.
  auto options = gc::Options{}.set<gc::OpenTelemetryTracingOption>(true);
  auto client = gcs::Client(options);

  auto writer = client.WriteObject(bucket_name, "quickstart.txt");
  writer << "Hello World!";
  writer.Close();
  if (!writer.metadata()) {
    std::cerr << "Error creating object: " << writer.metadata().status()
              << "\n";
    return 1;
  }
  std::cout << "Successfully created object: " << *writer.metadata() << "\n";

  auto reader = client.ReadObject(bucket_name, "quickstart.txt");
  if (!reader) {
    std::cerr << "Error reading object: " << reader.status() << "\n";
    return 1;
  }

  std::string contents{std::istreambuf_iterator<char>{reader}, {}};
  std::cout << contents << "\n";

  // The basic tracing configuration object goes out of scope. The collected
  // spans are flushed to Cloud Trace.

  return 0;
}
```

<!-- inject-quickstart-end -->

## More Information

- If you want to learn more about the [OpenTelemetry] for C++ SDKs, consult
  their [online documentation](https://opentelemetry-cpp.readthedocs.io/).
- [Reference doxygen documentation][doxygen-link] for each release of this
  library
- Detailed header comments in our [public `.h`][source-link] files
- The [Setting up your development environment] guide describes how to set up a
  C++ development environment in various platforms, including the Google Cloud
  C++ client libraries.

[cloud trace]: https://cloud.google.com/trace
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/opentelemetry/latest/
[opentelemetry]: https://opentelemetry.io/
[setting up your development environment]: https://cloud.google.com/cpp/docs/setup
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/opentelemetry
[traces]: https://opentelemetry.io/docs/concepts/observability-primer/#distributed-traces
