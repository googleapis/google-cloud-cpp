// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_OPENTELEMETRY_CONFIGURE_BASIC_TRACING_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_OPENTELEMETRY_CONFIGURE_BASIC_TRACING_H

#include "google/cloud/options.h"
#include "google/cloud/project.h"
#include "google/cloud/version.h"
#include <memory>

namespace google {
namespace cloud {
namespace otel {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
/// Implementation details for `ConfigureBasicTracing`.
class BasicTracingConfiguration {
 public:
  virtual ~BasicTracingConfiguration() = default;
};

/**
 * Configure the application for basic request tracing.
 *
 * This function configures basic request tracing to [Cloud Trace]. The
 * `google-cloud-cpp` libraries use [OpenTelemetry] to provide observability
 * into their operation at runtime.
 *
 * You do not need to add OpenTelemetry instrumentation to your code. The C++
 * client libraries are already instrumented and all sampled RPCs will be sent
 * to Cloud Trace. However, you may want to add instrumentation if multiple
 * RPCs are performed as part of a single logical "operation" in your
 * application.
 *
 * OpenTelemetry traces, including those reported by the C++ client libraries
 * start as soon as this function returns. Tracing stops when the object
 * returned by this function is deleted.
 *
 * OpenTelemetry is very configurable, supporting different sampling rates and
 * filters, multiple "exporters" to send the collected data to different
 * services, and multiple mechanisms to chain requests as they move from one
 * program to the next. We do not expect this function will meet the needs of
 * all applications. However, some applications will want a basic configuration
 * that works with Gooogle Cloud Trace.
 *
 * This function uses the [OpenTelemetry C++ API] to change the global trace
 * provider (`opentelemetry::trace::Provider::#SetTraceProvider()`). Do not use
 * this function if your application needs fine control over OpenTelemetry
 * settings.
 *
 * @note If you are using CMake as your build system, OpenTelemetry is not
 *     enabled by default in `google-cloud-cpp`. Please consult the build
 *     documentation to enable the additional libraries.
 *
 * @par Usage Example
 * @parblock
 * Change your build scripts to also build and link the library that provides
 * this function, as described in this library's [quickstart].
 *
 * Change your application to call this function once, for example in `main()`
 * as follows:
 *
 * @code
 * #include "google/cloud/opentelemetry/configure_basic_tracing.h"
 *
 * ...
 *
 * int main(...) {
 * ...
 *   auto tracing_project = std::string([TRACING PROJECT]);
 *   auto tracing = google::cloud::opentelemetry::ConfigureBasicTracing(
 *       google::cloud::Project(tracing_project));
 * }
 * @endcode
 *
 * Where `[TRACING PROJECT]` is the project id where you want to store the
 * traces.
 * @endparblock
 *
 * @par Permissions
 * @parblock
 * The principal (user or service account) running your application
 * will need `cloud.traces.patch` permissions on the project where you send
 * the traces. These permissions are typically granted as part of the
 * `roles/cloudtrace.agent` role. If the principal configured in your
 * [Application Default Credentials] does not have these permissions you will
 * need to provide a different set of credentials:
 *
 * @code
 *   auto credentials = google::cloud::MakeServiceAccountCredentials(...);
 *   auto tracing = google::cloud::opentelemetry::ConfigureBasicTracing(
 *       google::cloud::Project(tracing_project),
 *       google::cloud::Options{}
 *           .set<google::cloud::UnifiedCredentialsOption>(credentials));
 * @endcode
 * @endparblock
 *
 * @par Sampling Rate
 * @parblock
 * By default this function configures the application to trace all requests.
 * This is useful for troubleshooting, but it is excessive if you want to enable
 * tracing by default and use the results to gather latency statistics. To
 * reduce the sampling rate use `@ref BasicTracingRateOption`. If desired,
 * you can use an environment variable (or any other configuration source)
 * to initialize its value.
 *
 * @snippet samples.cc otel-basic-tracing-rate
 * @endparblock
 *
 * @par Troubleshooting
 * @parblock
 * By design, OpenTelemetry exporters fail silently. To troubleshoot problems,
 * [enable logging] in the client library. Errors during the export are logged
 * at a WARNING level.
 *
 * Look through the logs for mentions of "BatchWriteSpans". These mentions are
 * likely accompanied by actionable error messages.
 *
 * If "BatchWriteSpans" is not mentioned in the logs, the client library did not
 * attempt to export any traces. In this case, check that the project ID is not
 * empty and that the sample rate is high enough. Also ensure that OpenTelemetry
 * tracing is enabled in the library.
 *
 * See also: https://cloud.google.com/trace/docs/troubleshooting#no-data
 * @endparblock
 *
 * @param project the project to send the traces to.
 * @param options how to configure the traces. The configuration parameters
 *     include `@ref BasicTracingRateOption`,
 *     `@ref google::cloud::UnifiedCredentialsOption`.
 *
 * @see https://cloud.google.com/trace/docs/iam for more information about IAM
 *     permissions for Cloud Trace.
 *
 * [Cloud Trace]: https://cloud.google.com/trace
 * [OpenTelemetry]: https://opentelemetry.io
 * [OpenTelemetry C++ API]: https://opentelemetry-cpp.readthedocs.io/en/latest/
 * [Application Default Credentials]:
 * https://cloud.google.com/docs/authentication#adc
 * [enable logging]:
 * https://cloud.google.com/cpp/docs/reference/common/latest/logging#enabling-logs
 * [quickstart]:
 * https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/opentelemetry/quickstart/README.md#opentelemetry-dependency
 */
std::unique_ptr<BasicTracingConfiguration> ConfigureBasicTracing(
    Project project, Options options = {});

/**
 * Configure the tracing rate for basic tracing.
 *
 * @see `@ref ConfigureBasicTracing()` for more information.
 */
struct BasicTracingRateOption {
  using Type = double;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_OPENTELEMETRY_CONFIGURE_BASIC_TRACING_H
