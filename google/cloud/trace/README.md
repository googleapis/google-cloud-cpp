# Stackdriver Trace API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Stackdriver Trace API][cloud-service-docs], a service to send application trace
data to Stackdriver Trace for viewing. This library is used to interact with the
Trace API directly. If you are looking to instrument your application for
Stackdriver Trace, we recommend using [OpenTelemetry](https://opentelemetry.io)
or a similar framework.

While this library is **GA**, please note that the Google Cloud C++ client libraries do **not** follow
[Semantic Versioning](https://semver.org/).

## Supported Platforms

* Windows, macOS, Linux
* C++14 (and higher) compilers (we test with GCC >= 5.4, Clang >= 6.0, and
  MSVC >= 2017)
* Environments with or without exceptions
* Bazel (>= 4.0) and CMake (>= 3.5) builds

## Documentation

* Official documentation about the [Stackdriver Trace API][cloud-service-docs] service
* [Reference doxygen documentation][doxygen-link] for each release of this
  client library
* Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/trace
[doxygen-link]: https://googleapis.dev/cpp/google-cloud-trace/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/trace

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->
```cc
#include "google/cloud/trace/trace_client.h"
#include "google/cloud/project.h"
#include <google/protobuf/util/time_util.h>
#include <iostream>
#include <random>
#include <stdexcept>
#include <thread>

std::string RandomHexDigits(std::mt19937_64& gen, int count) {
  auto const digits = std::string{"0123456789abcdef"};
  std::string sample;
  std::generate_n(std::back_inserter(sample), count, [&] {
    auto n = digits.size() - 1;
    return digits[std::uniform_int_distribution<std::size_t>(0, n)(gen)];
  });
  return sample;
}

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  namespace trace = ::google::cloud::trace;
  namespace v2 = ::google::devtools::cloudtrace::v2;
  using ::google::protobuf::util::TimeUtil;

  auto client = trace::TraceServiceClient(trace::MakeTraceServiceConnection());

  // Create a span ID using some random hex digits.
  auto gen = std::mt19937_64(std::random_device{}());
  v2::Span span;
  auto span_id = RandomHexDigits(gen, 16);
  span.set_name(std::string{"projects/"} + argv[1] + "/traces/" +
                RandomHexDigits(gen, 32) + "/spans/" + span_id);
  span.set_span_id(std::move(span_id));
  *span.mutable_start_time() = TimeUtil::GetCurrentTime();
  // Simulate a call using a small sleep
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  *span.mutable_end_time() = TimeUtil::GetCurrentTime();

  auto response = client.CreateSpan(span);
  if (!response) throw std::runtime_error(response.status().message());
  std::cout << response->DebugString() << "\n";

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << "\n";
  return 1;
}
```
<!-- inject-quickstart-end -->

* Packaging maintainers or developers who prefer to install the library in a
  fixed directory (such as `/usr/local` or `/opt`) should consult the
  [packaging guide](/doc/packaging.md).
* Developers wanting to use the libraries as part of a larger CMake or Bazel
  project should consult the [quickstart guides](#quickstart) for the library
  or libraries they want to use.
* Developers wanting to compile the library just to run some of the examples or
  tests should read the current document.
* Contributors and developers to `google-cloud-cpp` should consult the guide to
  [setup a development workstation][howto-setup-dev-workstation].

[howto-setup-dev-workstation]: /doc/contributor/howto-guide-setup-development-workstation.md

## Contributing changes

See [`CONTRIBUTING.md`](/CONTRIBUTING.md) for details on how to
contribute to this project, including how to build and test your changes
as well as how to properly format your code.

## Licensing

Apache 2.0; see [`LICENSE`](/LICENSE) for details.
