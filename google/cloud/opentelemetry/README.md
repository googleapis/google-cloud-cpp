# C++ OpenTelemetry Exporters for Google Cloud Platform services.

:construction:

This library is **experimental**. Its APIs are subject to change without notice.

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/opentelemetry/configure_basic_tracing.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/internal/opentelemetry_options.h"
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
  auto options =
      gc::Options{}.set<gc::internal::OpenTelemetryTracingOption>(true);
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

## Build and Install

- Packaging maintainers or developers who prefer to install the library in a
  fixed directory (such as `/usr/local` or `/opt`) should consult the
  [packaging guide](/doc/packaging.md).
- Developers who prefer using a package manager such as
  [vcpkg](https://vcpkg.io), or [Conda](https://conda.io), should follow the
  instructions for their package manager.
- Developers wanting to use the libraries as part of a larger CMake or Bazel
  project should consult the [quickstart guides](#quickstart) for the library
  or libraries they want to use.
- Developers wanting to compile the library just to run some examples or
  tests should read the project's top-level
  [README](/README.md#building-and-installing).
- Contributors and developers to `google-cloud-cpp` should consult the guide to
  [set up a development workstation][howto-setup-dev-workstation].

## More Information

- If you want to learn more about the [OpenTelemetry] for C++ SDKs, consult
  their [online documentation](https://opentelemetry-cpp.readthedocs.io/).
- [Reference doxygen documentation][doxygen-link] for each release of this client library
- Detailed header comments in our [public `.h`][source-link] files

[doxygen-link]: https://googleapis.dev/cpp/google-cloud-storage/latest/
[howto-setup-dev-workstation]: /doc/contributor/howto-guide-setup-development-workstation.md
[opentelemetry]: https://opentelemetry.io/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/storage
