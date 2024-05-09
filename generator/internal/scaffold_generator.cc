// Copyright 2021 Google LLC
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

#include "generator/internal/scaffold_generator.h"
#include "generator/internal/codegen_utils.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/absl_str_replace_quiet.h"
#include "google/cloud/internal/filesystem.h"
#include "google/cloud/log.h"
#include "absl/strings/str_split.h"
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <iterator>
#include <regex>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

struct ProductPath {
  std::string prefix;
  std::string library_name;
  std::string service_subdirectory;
};

ProductPath ParseProductPath(std::string const& product_path) {
  std::vector<absl::string_view> v =
      absl::StrSplit(product_path, '/', absl::SkipEmpty());
  if (v.empty()) return {};
  auto make_result = [&v](auto it) -> ProductPath {
    return {
        absl::StrJoin(v.begin(), it, "/"),
        std::string{*it},
        absl::StrJoin(std::next(it), v.end(), "/"),
    };
  };
  // This is the case for our production code.
  if (v.size() > 2 && v[0] == "google" && v[1] == "cloud") {
    return make_result(std::next(v.begin(), 2));
  }
  // "golden" is a special library name used in our golden testing.
  auto it = std::find(v.begin(), v.end(), "golden");
  if (it != v.end()) return make_result(it);
  // Else, just assume the last element is the library.
  return make_result(std::prev(v.end()));
}

std::string FormatCloudServiceDocsLink(
    std::map<std::string, std::string> const& vars) {
  return absl::StrCat("[cloud-service-docs]: ",
                      vars.find("documentation_uri") == vars.end()
                          ? "https://cloud.google.com/$site_root$ [EDIT HERE]"
                          : "$documentation_uri$",
                      "\n");
}

}  // namespace

auto constexpr kApiIndexFilename = "api-index-v1.json";
auto constexpr kWorkspaceTemplate = "WORKSPACE.bazel";

std::string LibraryName(std::string const& product_path) {
  return ParseProductPath(product_path).library_name;
}

std::string LibraryPath(std::string const& product_path) {
  auto parsed = ParseProductPath(product_path);
  return absl::StrCat(parsed.prefix, "/", parsed.library_name, "/");
}

std::string ServiceSubdirectory(std::string const& product_path) {
  auto parsed = ParseProductPath(product_path);
  if (parsed.service_subdirectory.empty()) return std::string{};
  return absl::StrCat(parsed.service_subdirectory, "/");
}

std::string OptionsGroup(std::string const& product_path) {
  return absl::StrCat(
      absl::StrReplaceAll(LibraryPath(product_path), {{"/", "-"}}), "options");
}

std::string SiteRoot(
    google::cloud::cpp::generator::ServiceConfiguration const& service) {
  // TODO(#7605) - get a configurable source for this
  return LibraryName(service.product_path());
}

nlohmann::json LoadApiIndex(std::string const& googleapis_path) {
  auto const api_index_path = googleapis_path + "/" + kApiIndexFilename;
  auto status = google::cloud::internal::status(api_index_path);
  if (!exists(status)) {
    GCP_LOG(WARNING) << "Cannot find API index file (" << api_index_path << ")";
    return {};
  }
  std::ifstream is(api_index_path);
  auto index = nlohmann::json::parse(is, nullptr, false);
  if (index.is_null()) {
    GCP_LOG(WARNING) << "Cannot parse API index file (" << api_index_path
                     << ")";
    return nlohmann::json{{"apis", std::vector<nlohmann::json>{}}};
  }
  if (!index.contains("apis")) {
    GCP_LOG(WARNING) << "Missing `apis` field in API index file ("
                     << api_index_path << ")";
    return nlohmann::json{{"apis", std::vector<nlohmann::json>{}}};
  }
  return index;
}

std::map<std::string, std::string> ScaffoldVars(
    std::string const& yaml_root, nlohmann::json const& index,
    google::cloud::cpp::generator::ServiceConfiguration const& service,
    bool experimental) {
  std::map<std::string, std::string> vars;
  for (auto const& api : index["apis"]) {
    if (!api.contains("directory")) continue;
    auto const directory = api["directory"].get<std::string>() + "/";
    if (!absl::StartsWith(service.service_proto_path(), directory)) continue;
    vars.emplace("id", api.value("id", ""));
    vars.emplace("title", api.value("title", ""));
    vars.emplace("description", api.value("description", ""));
    vars.emplace("directory", api.value("directory", ""));
    vars.emplace("service_config_yaml_name",
                 absl::StrCat(api.value("directory", ""), "/",
                              api.value("configFile", "")));
    vars.emplace("nameInServiceConfig", api.value("nameInServiceConfig", ""));
  }
  if (!service.override_service_config_yaml_name().empty()) {
    vars.emplace("service_config_yaml_name",
                 service.override_service_config_yaml_name());
  }
  auto const library = LibraryName(service.product_path());
  vars["copyright_year"] = service.initial_copyright_year();
  vars["library"] = library;
  vars["product_namespace"] = absl::StrReplaceAll(
      absl::StripSuffix(
          absl::StrCat(library, "/",
                       ServiceSubdirectory(service.product_path())),
          "/"),
      {{"/", "_"}});
  vars["product_options_page"] = OptionsGroup(service.product_path());
  vars["service_subdirectory"] = ServiceSubdirectory(service.product_path());
  vars["site_root"] = SiteRoot(service);
  vars["experimental"] = experimental ? " EXPERIMENTAL" : "";
  vars["library_prefix"] = experimental ? "experimental-" : "";
  vars["construction"] = experimental ? "\n:construction:\n" : "";
  vars["status"] =
      experimental ? "This library is **experimental**. Its APIs are subject "
                     "to change without notice.\n\nPlease,"
                   : "While this library is **GA**, please";

  // Find out if the service config YAML is configured.
  auto path = ServiceConfigYamlPath(yaml_root, vars);
  if (path.empty()) {
    GCP_LOG(WARNING) << "Missing directory and/or YAML config file name for: "
                     << service.service_proto_path();
    return vars;
  }

  // Try to load the service config YAML file. On failure just return the
  // existing vars.
  auto status = google::cloud::internal::status(path);
  if (!exists(status)) {
    GCP_LOG(WARNING) << "Cannot find YAML service config file (" << path
                     << ") for: " << service.service_proto_path();
    return vars;
  }
  auto config = YAML::LoadFile(path);
  if (config.Type() != YAML::NodeType::Map) {
    GCP_LOG(WARNING) << "Error loading YAML config file (" << path
                     << ") for: " << service.service_proto_path()
                     << "  error=" << config.Type();
    return vars;
  }
  auto publishing = config["publishing"];
  // This error is too common at the moment. Most libraries lack a
  // 'publishing' section.
  if (publishing.Type() != YAML::NodeType::Map) return vars;

  for (std::string const name : {
           "api_short_name",
           "documentation_uri",
           "new_issue_uri",
       }) {
    auto node = publishing[name];
    if (node.Type() != YAML::NodeType::Scalar) continue;
    auto value = node.as<std::string>();
    if (value.empty()) continue;
    vars[name] = std::move(value);
  }
  // The YAML configuration includes a link to create new issues. If possible,
  // convert that to a link to list issues, which is what we want to generate.
  auto l = vars.find("new_issue_uri");
  if (l != vars.end()) {
    auto issue_tracker = l->second;
    auto const re = std::regex(
        R"re((https://issuetracker.google.com/issues).*[^a-z]component=([0-9]*).*)re");
    std::smatch match;
    if (std::regex_match(issue_tracker, match, re)) {
      issue_tracker =
          absl::StrCat(match[1].str(), "?q=componentid:", match[2].str(), "%20",
                       "status=open");
    }
    vars["issue_tracker"] = issue_tracker;
  }

  return vars;
}

std::string ServiceConfigYamlPath(
    std::string const& root, std::map<std::string, std::string> const& vars) {
  auto const name = vars.find("service_config_yaml_name");
  if (name == vars.end()) return {};
  return absl::StrCat(root, "/", name->second);
}

void GenerateMetadata(
    std::map<std::string, std::string> const& vars,
    std::string const& output_path,
    google::cloud::cpp::generator::ServiceConfiguration const& service,
    bool allow_placeholders) {
  auto const destination = [&]() {
    MakeDirectory(output_path);
    auto d = output_path + "/" + LibraryPath(service.product_path());
    MakeDirectory(d);
    auto l = vars.find("service_subdirectory");
    if (l == vars.end()) return d;
    if (l->second.empty()) return d;
    d += l->second;
    MakeDirectory(d);
    return d;
  }();

  nlohmann::json metadata{
      {"language", "cpp"},
      {"repo", "googleapis/google-cloud-cpp"},
      {"release_level", service.experimental() ? "preview" : "stable"},
      // In other languages this is the name of the package. In C++ there are
      // many package managers, and this seems to be the most common name.
      {"distribution_name", "google-cloud-cpp"},
      // This seems to be largely unused, but better to put a value.
      {"requires_billing", true},
      // Assume the library is automatically generated. For hand-crafted
      // libraries we will set `omit_repo_metadata: true` in
      // generator_config.textproto.
      {"library_type", "GAPIC_AUTO"},
  };
  auto library = vars.find("library");
  if (library == vars.end()) {
    GCP_LOG(WARNING) << "Cannot find field `library` in configuration vars for:"
                     << service.service_proto_path();
    return;
  }
  metadata["client_documentation"] =
      "https://cloud.google.com/cpp/docs/reference/" + library->second +
      "/latest";

  struct MapVars {
    std::string metadata_name;
    std::string var_name;
  } map_vars[] = {
      {"name_pretty", "title"},
      {"api_id", "nameInServiceConfig"},
      {"product_documentation", "documentation_uri"},
      {"issue_tracker", "issue_tracker"},
  };
  for (auto const& kv : map_vars) {
    auto const l = vars.find(kv.var_name);
    if (l == vars.end()) {
      // At the moment, too many proto directories lack a `publishing` section
      // in their YAML file.
      if (!allow_placeholders) return;
      metadata[kv.metadata_name] = "EDIT HERE: missing data";
      continue;
    }
    metadata[kv.metadata_name] = l->second;
  }
  std::vector<std::string> id_components =
      absl::StrSplit(metadata.value("api_id", ""), absl::MaxSplits('.', 1));
  if (id_components.empty()) return;
  metadata["api_shortname"] = std::string(id_components[0]);

  std::ofstream(destination + ".repo-metadata.json")
      << metadata.dump(4) << "\n";
}

void GenerateScaffold(
    std::map<std::string, std::string> const& vars,
    std::string const& scaffold_templates_path, std::string const& output_path,
    google::cloud::cpp::generator::ServiceConfiguration const& service) {
  using Generator = std::function<void(
      std::ostream&, std::map<std::string, std::string> const&)>;
  struct Destination {
    std::string name;
    Generator generator;
  } files[] = {
      {"README.md", GenerateReadme},
      {"BUILD.bazel", GenerateBuild},
      {"CMakeLists.txt", GenerateCMakeLists},
      {"doc/main.dox", GenerateDoxygenMainPage},
      {"doc/environment-variables.dox", GenerateDoxygenEnvironmentPage},
      {"doc/override-authentication.dox", GenerateOverrideAuthenticationPage},
      {"doc/override-endpoint.dox", GenerateOverrideEndpointPage},
      {"doc/override-retry-policies.dox", GenerateOverrideRetryPoliciesPage},
      {"doc/options.dox", GenerateDoxygenOptionsPage},
      {"quickstart/README.md", GenerateQuickstartReadme},
      {"quickstart/quickstart.cc", GenerateQuickstartSkeleton},
      {"quickstart/CMakeLists.txt", GenerateQuickstartCMake},
      {"quickstart/Makefile", GenerateQuickstartMakefile},
      {"quickstart/BUILD.bazel", GenerateQuickstartBuild},
      {"quickstart/.bazelrc", GenerateQuickstartBazelrc},
  };

  MakeDirectory(output_path + "/");
  auto const destination =
      output_path + "/" + LibraryPath(service.product_path());
  MakeDirectory(destination);
  MakeDirectory(destination + "doc/");
  MakeDirectory(destination + "quickstart/");
  for (auto const& f : files) {
    std::ofstream os(destination + f.name);
    f.generator(os, vars);
  }
  std::ifstream is(scaffold_templates_path + kWorkspaceTemplate);
  auto const contents = std::string{std::istreambuf_iterator<char>(is), {}};
  std::ofstream os(destination + "quickstart/WORKSPACE.bazel");
  GenerateQuickstartWorkspace(os, vars, contents);
}

void GenerateReadme(std::ostream& os,
                    std::map<std::string, std::string> const& variables) {
  auto constexpr kText1 = R"""(# $title$ C++ Client Library
$construction$
This directory contains an idiomatic C++ client library for the
[$title$][cloud-service-docs].

$description$

$status$ note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->
<!-- inject-quickstart-end -->

## More Information

* Official documentation about the [$title$][cloud-service-docs] service
* [Reference doxygen documentation][doxygen-link] for each release of this
  client library
* Detailed header comments in our [public `.h`][source-link] files
)""";

  auto constexpr kText2 =
      R"""([doxygen-link]: https://cloud.google.com/cpp/docs/reference/$library$/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/$library$
)""";
  google::protobuf::io::OstreamOutputStream output(&os);
  google::protobuf::io::Printer printer(&output, '$');
  printer.Print(
      variables,
      absl::StrCat(kText1, FormatCloudServiceDocsLink(variables), kText2)
          .c_str());
}

void GenerateBuild(std::ostream& os,
                   std::map<std::string, std::string> const& variables) {
  auto constexpr kText = R"""(# Copyright $copyright_year$ Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@google_cloud_cpp//bazel:gapic.bzl", "cc_gapic_library")

package(default_visibility = ["//visibility:private"])

licenses(["notice"])  # Apache 2.0

service_dirs = ["$service_subdirectory$"]

googleapis_deps = [
    "@com_google_googleapis//$directory$:$library$_cc_grpc",
]

cc_gapic_library(
    name = "$library$",
    googleapis_deps = googleapis_deps,
    service_dirs = service_dirs,
)
)""";
  google::protobuf::io::OstreamOutputStream output(&os);
  google::protobuf::io::Printer printer(&output, '$');
  printer.Print(variables, kText);
}

void GenerateCMakeLists(std::ostream& os,
                        std::map<std::string, std::string> const& variables) {
  auto constexpr kText = R"""(# ~~~
# Copyright $copyright_year$ Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ~~~

include(GoogleCloudCppLibrary)

google_cloud_cpp_add_gapic_library($library$ "$title$"$experimental$
    SERVICE_DIRS "$service_subdirectory$")

if (BUILD_TESTING AND GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    add_executable($library$_quickstart "quickstart/quickstart.cc")
    target_link_libraries($library$_quickstart
                          PRIVATE google-cloud-cpp::$library_prefix$$library$)
    google_cloud_cpp_add_common_options($library$_quickstart)
    add_test(
        NAME $library$_quickstart
        COMMAND cmake -P "$${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
                $$<TARGET_FILE:$library$_quickstart> GOOGLE_CLOUD_PROJECT
                GOOGLE_CLOUD_CPP_TEST_REGION # EDIT HERE
    )
    set_tests_properties($library$_quickstart
                         PROPERTIES LABELS "integration-test;quickstart")
endif ()
)""";
  google::protobuf::io::OstreamOutputStream output(&os);
  google::protobuf::io::Printer printer(&output, '$');
  printer.Print(variables, kText);
}

void GenerateDoxygenMainPage(
    std::ostream& os, std::map<std::string, std::string> const& variables) {
  auto constexpr kText1 = R"""(/*!

@mainpage $title$ C++ Client Library

An idiomatic C++ client library for the [$title$][cloud-service-docs].

$description$

$status$ note that the Google Cloud C++ client libraries do **not** follow
[Semantic Versioning](https://semver.org/).

@tableofcontents{HTML:2}

## Quickstart

The following shows the code that you'll run in the
`google/cloud/$library$/quickstart/` directory,
which should give you a taste of the $title$ C++ client library API.

@snippet quickstart.cc all

## Main classes

<!-- inject-client-list-start -->
<!-- inject-client-list-end -->

## More Information

- @ref common-error-handling - describes how the library reports errors.
- @ref $library$-override-endpoint - describes how to override the default
  endpoint.
- @ref $library$-override-authentication - describes how to change the
  authentication credentials used by the library.
- @ref $library$-override-retry - describes how to change the default retry
  policies.
- @ref $library$-env - describes environment variables that can configure the
  behavior of the library.
)""";

  auto constexpr kText2 = R"""(
*/
)""";
  google::protobuf::io::OstreamOutputStream output(&os);
  google::protobuf::io::Printer printer(&output, '$');
  printer.Print(
      variables,
      absl::StrCat(kText1, FormatCloudServiceDocsLink(variables), kText2)
          .c_str());
}

void GenerateDoxygenEnvironmentPage(
    std::ostream& os, std::map<std::string, std::string> const& variables) {
  auto constexpr kText = R"""(/*!

@page $library$-env Environment Variables

A number of environment variables can be used to configure the behavior of
the library. There are also functions to configure this behavior in code. The
environment variables are convenient when troubleshooting problems.

@section $library$-env-endpoint Endpoint Overrides

<!-- inject-endpoint-env-vars-start -->
<!-- inject-endpoint-env-vars-end -->

@see google::cloud::EndpointOption

@section $library$-env-logging Logging

`GOOGLE_CLOUD_CPP_ENABLE_TRACING=rpc`: turns on tracing for most gRPC
calls. The library injects an additional Stub decorator that prints each gRPC
request and response.  Unless you have configured your own logging backend,
you should also set `GOOGLE_CLOUD_CPP_ENABLE_CLOG` to produce any output on
the program's console.

@see google::cloud::TracingComponentsOption

`GOOGLE_CLOUD_CPP_TRACING_OPTIONS=...`: modifies the behavior of gRPC tracing,
including whether messages will be output on multiple lines, or whether
string/bytes fields will be truncated.

@see google::cloud::GrpcTracingOptionsOption

`GOOGLE_CLOUD_CPP_ENABLE_CLOG=yes`: turns on logging in the library, basically
the library always "logs" but the logging infrastructure has no backend to
actually print anything until the application sets a backend or they set this
environment variable.

@see google::cloud::LogBackend
@see google::cloud::LogSink

@section $library$-env-project Setting the Default Project

`GOOGLE_CLOUD_PROJECT=...`: is used in examples and integration tests to
configure the GCP project. This has no effect in the library.

*/
)""";
  google::protobuf::io::OstreamOutputStream output(&os);
  google::protobuf::io::Printer printer(&output, '$');
  printer.Print(variables, kText);
}

void GenerateOverrideAuthenticationPage(
    std::ostream& os, std::map<std::string, std::string> const& variables) {
  auto constexpr kText =
      R"""(/*!
@page $library$-override-authentication How to Override the Authentication Credentials

Unless otherwise configured, the client libraries use
[Application Default Credentials] to authenticate with Google Cloud Services.
While this works for most applications, in some cases you may need to override
this default. You can do so by providing the
[UnifiedCredentialsOption](@ref google::cloud::UnifiedCredentialsOption)
The following example shows how to explicitly load a service account key file:

<!-- inject-service-account-snippet-start -->
<!-- inject-service-account-snippet-end -->

Keep in mind that we chose this as an example because it is relatively easy to
understand. Consult the [Best practices for managing service account keys]
guide for more details.

@see @ref guac - for more information on the factory functions to create
`google::cloud::Credentials` objects.

[Best practices for managing service account keys]: https://cloud.google.com/iam/docs/best-practices-for-managing-service-account-keys
[Application Default Credentials]: https://cloud.google.com/docs/authentication#adc

*/

// <!-- inject-authentication-pages-start -->
// <!-- inject-authentication-pages-end -->
)""";
  google::protobuf::io::OstreamOutputStream output(&os);
  google::protobuf::io::Printer printer(&output, '$');
  printer.Print(variables, kText);
}

void GenerateOverrideEndpointPage(
    std::ostream& os, std::map<std::string, std::string> const& variables) {
  auto constexpr kText = R"""(/*!
@page $library$-override-endpoint How to Override the Default Endpoint

In some cases, you may need to override the default endpoint used by the client
library. Use the
[EndpointOption](@ref google::cloud::EndpointOption) when initializing the
client library to change this default.

<!-- inject-endpoint-snippet-start -->
<!-- inject-endpoint-snippet-end -->

*/

// <!-- inject-endpoint-pages-start -->
// <!-- inject-endpoint-pages-end -->
)""";
  google::protobuf::io::OstreamOutputStream output(&os);
  google::protobuf::io::Printer printer(&output, '$');
  printer.Print(variables, kText);
}

void GenerateOverrideRetryPoliciesPage(
    std::ostream& os, std::map<std::string, std::string> const& variables) {
  auto constexpr kText = R"""(/*!
@page $library$-override-retry Override Retry, Backoff, and Idempotency Policies

When it is safe to do so, the library automatically retries requests that fail
due to a transient error. The library then uses [exponential backoff] to backoff
before trying again. Which operations are considered safe to retry, which
errors are treated as transient failures, the details of the exponential backoff
algorithm, and for how long the library retries are all configurable via
policies.

This document provides examples showing how to override the default policies.

The policies can be set when the `*Connection` object is created. The library
provides default policies for any policy that is not set. The application can
also override some (or all) policies when the `*Client` object is created. This
can be useful if multiple `*Client` objects share the same `*Connection` object,
but you want different retry behavior in some of the clients. Finally, the
application can override some retry policies when calling a specific member
function.

The library uses three different options to control the retry loop. The options
have per-client names.

@section $library$-override-retry-retry-policy Configuring the transient errors and retry duration

The `*RetryPolicyOption` controls:

- Which errors are to be treated as transient errors.
- How long the library will keep retrying transient errors.

You can provide your own class for this option. The library also provides two
built-in policies:

- `*LimitedErrorCountRetryPolicy`: stops retrying after a specified number
  of transient errors.
- `*LimitedTimeRetryPolicy`: stops retrying after a specified time.

Note that a library may have more than one version of these classes. Their name
match the `*Client` and `*Connection` object they are intended to be used
with. Some `*Client` objects treat different error codes as transient errors.
In most cases, only [kUnavailable](@ref google::cloud::StatusCode) is treated
as a transient error.

@section $library$-override-retry-backoff-policy Controlling the backoff algorithm

The `*BackoffPolicyOption` controls how long the client library will wait
before retrying a request that failed with a transient error. You can provide
your own class for this option.

The only built-in backoff policy is
[`ExponentialBackoffPolicy`](@ref google::cloud::ExponentialBackoffPolicy).
This class implements a truncated exponential backoff algorithm, with jitter.
In summary, it doubles the current backoff time after each failure. The actual
backoff time for an RPC is chosen at random, but never exceeds the current
backoff. The current backoff is doubled after each failure, but never exceeds
(or is "truncated") if it reaches a prescribed maximum.

@section $library$-override-retry-idempotency-policy Controlling which operations are retryable

The `*IdempotencyPolicyOption` controls which requests are retryable, as some
requests are never safe to retry.

Only one built-in idempotency policy is provided by the library. The name
matches the name of the client it is intended for. For example, `FooBarClient`
will use `FooBarIdempotencyPolicy`. This policy is very conservative.

@section $library$-override-retry-example Example

<!-- inject-retry-snippet-start -->
<!-- inject-retry-snippet-end -->

@section $library$-override-retry-more-information More Information

@see google::cloud::Options
@see google::cloud::BackoffPolicy
@see google::cloud::ExponentialBackoffPolicy

[exponential backoff]: https://en.wikipedia.org/wiki/Exponential_backoff

*/

// <!-- inject-retry-pages-start -->
// <!-- inject-retry-pages-end -->
)""";
  google::protobuf::io::OstreamOutputStream output(&os);
  google::protobuf::io::Printer printer(&output, '$');
  printer.Print(variables, kText);
}

void GenerateDoxygenOptionsPage(
    std::ostream& os, std::map<std::string, std::string> const& variables) {
  auto constexpr kText = R"""(/*!
@defgroup $product_options_page$ $title$ Configuration Options

This library uses the same mechanism (`google::cloud::Options`) and the common
[options](@ref options) as all other C++ client libraries for its configuration.
Some `*Option` classes, which are only used in this library, are documented in
this page.

@see @ref options - for an overview of client library configuration.
*/
)""";
  google::protobuf::io::OstreamOutputStream output(&os);
  google::protobuf::io::Printer printer(&output, '$');
  printer.Print(variables, kText);
}

void GenerateQuickstartReadme(
    std::ostream& os, std::map<std::string, std::string> const& variables) {
  auto constexpr kText =
      R"""(# HOWTO: using the $title$ C++ client in your project

This directory contains small examples showing how to use the $title$ C++
client library in your own project. These instructions assume that you have
some experience as a C++ developer and that you have a working C++ toolchain
(compiler, linker, etc.) installed on your platform.

- Packaging maintainers or developers who prefer to install the library in a
  fixed directory (such as `/usr/local` or `/opt`) should consult the
  [packaging guide](/doc/packaging.md).
- Developers who prefer using a package manager such as
  [vcpkg](https://vcpkg.io), or [Conda](https://conda.io), should follow the
  instructions for their package manager.
- Developers wanting to use the libraries as part of a larger CMake or Bazel
  project should consult the current document. Note that there are similar
  documents for each library in their corresponding directories.
- Developers wanting to compile the library just to run some examples or
  tests should consult the
  [building and installing](/README.md#building-and-installing) section of the
  top-level README file.
- Contributors and developers to `google-cloud-cpp` should consult the guide to
  [set up a development workstation][howto-setup-dev-workstation].

[howto-setup-dev-workstation]: /doc/contributor/howto-guide-setup-development-workstation.md

## Before you begin

To run the quickstart examples you will need a working Google Cloud Platform
(GCP) project. The [quickstart][quickstart-link] covers the necessary
steps in detail.

## Configuring authentication for the C++ Client Library

Like most Google Cloud Platform (GCP) services, $title$ requires that
your application authenticates with the service before accessing any data. If
you are not familiar with GCP authentication please take this opportunity to
review the [Authentication Overview][authentication-quickstart]. This library
uses the `GOOGLE_APPLICATION_CREDENTIALS` environment variable to find the
credentials file. For example:

| Shell              | Command                                        |
| :----------------- | ---------------------------------------------- |
| Bash/zsh/ksh/etc.  | `export GOOGLE_APPLICATION_CREDENTIALS=[PATH]` |
| sh                 | `GOOGLE_APPLICATION_CREDENTIALS=[PATH];`<br> `export GOOGLE_APPLICATION_CREDENTIALS` |
| csh/tsch           | `setenv GOOGLE_APPLICATION_CREDENTIALS [PATH]` |
| Windows Powershell | `$$env:GOOGLE_APPLICATION_CREDENTIALS=[PATH]`   |
| Windows cmd.exe    | `set GOOGLE_APPLICATION_CREDENTIALS=[PATH]`    |

Setting this environment variable is the recommended way to configure the
authentication preferences, though if the environment variable is not set, the
library searches for a credentials file in the same location as the [Cloud
SDK](https://cloud.google.com/sdk/). For more information about *Application
Default Credentials*, see
https://cloud.google.com/docs/authentication/production

## Using with Bazel

> :warning: If you are using Windows or macOS there are additional instructions
> at the end of this document.

1. Install Bazel using [the instructions][bazel-install] from the `bazel.build`
   website.

1. Compile this example using Bazel:

   ```bash
   cd $$HOME/google-cloud-cpp/google/cloud/$library$/quickstart
   bazel build ...
   ```

   Note that Bazel automatically downloads and compiles all dependencies of the
   project. As it is often the case with C++ libraries, compiling these
   dependencies may take several minutes.

1. Run the example, changing the placeholder(s) to appropriate values:

   ```bash
   bazel run :quickstart -- [...]
   ```

## Using with CMake

> :warning: If you are using Windows or macOS there are additional instructions
> at the end of this document.

1. Install CMake. The package managers for most Linux distributions include a
   package for CMake. Likewise, you can install CMake on Windows using a package
   manager such as [chocolatey][choco-cmake-link], and on macOS using
   [homebrew][homebrew-cmake-link]. You can also obtain the software directly
   from the [cmake.org](https://cmake.org/download/).

1. Install the dependencies with your favorite tools. As an example, if you use
   [vcpkg](https://github.com/Microsoft/vcpkg.git):

   ```bash
   cd $$HOME/vcpkg
   ./vcpkg install google-cloud-cpp[core,$library$]
   ```

   Note that, as it is often the case with C++ libraries, compiling these
   dependencies may take several minutes.

1. Configure CMake, if necessary, configure the directory where you installed
   the dependencies:

   ```bash
   cd $$HOME/google-cloud-cpp/google/cloud/$library$/quickstart
   cmake -S . -B .build -DCMAKE_TOOLCHAIN_FILE=$$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake
   cmake --build .build
   ```

1. Run the example, changing the placeholder(s) to appropriate values:

   ```bash
   .build/quickstart [...]
   ```

## Platform Specific Notes

### macOS

gRPC [requires][grpc-roots-pem-bug] an environment variable to configure the
trust store for SSL certificates, you can download and configure this using:

```bash
curl -Lo roots.pem https://pki.google.com/roots.pem
export GRPC_DEFAULT_SSL_ROOTS_FILE_PATH="$$PWD/roots.pem"
```

### Windows

Bazel tends to create very long file names and paths. You may need to use a
short directory to store the build output, such as `c:\b`, and instruct Bazel
to use it via:

```shell
bazel --output_user_root=c:\b build ...
```

gRPC [requires][grpc-roots-pem-bug] an environment variable to configure the
trust store for SSL certificates, you can download and configure this using:

```console
@powershell -NoProfile -ExecutionPolicy unrestricted -Command ^
    (new-object System.Net.WebClient).Downloadfile( ^
        'https://pki.google.com/roots.pem', 'roots.pem')
set GRPC_DEFAULT_SSL_ROOTS_FILE_PATH=%cd%\roots.pem
```

[bazel-install]: https://docs.bazel.build/versions/main/install.html
[quickstart-link]: https://cloud.google.com/$site_root$/docs/quickstart
[grpc-roots-pem-bug]: https://github.com/grpc/grpc/issues/16571
[choco-cmake-link]: https://chocolatey.org/packages/cmake
[homebrew-cmake-link]: https://formulae.brew.sh/formula/cmake
[cmake-download-link]: https://cmake.org/download/
[authentication-quickstart]: https://cloud.google.com/docs/authentication/getting-started 'Authentication Getting Started'
)""";
  google::protobuf::io::OstreamOutputStream output(&os);
  google::protobuf::io::Printer printer(&output, '$');
  printer.Print(variables, kText);
}

void GenerateQuickstartSkeleton(
    std::ostream& os, std::map<std::string, std::string> const& variables) {
  auto constexpr kText = R"""(// Copyright $copyright_year$ Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! [all]
#include "google/cloud/$library$/$service_subdirectory$ EDIT HERE _client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], argv[2]);

  namespace $library$ = ::google::cloud::$product_namespace$;
  auto client = $library$::ServiceClient(
      $library$::MakeServiceConnection());  // EDIT HERE

  for (auto r : client.List/*EDIT HERE*/(location.FullName())) {
    if (!r) throw std::move(r).status();
    std::cout << r->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
//! [all]
)""";
  google::protobuf::io::OstreamOutputStream output(&os);
  google::protobuf::io::Printer printer(&output, '$');
  printer.Print(variables, kText);
}

void GenerateQuickstartCMake(
    std::ostream& os, std::map<std::string, std::string> const& variables) {
  auto constexpr kText = R"""(# Copyright $copyright_year$ Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

# This file shows how to use the $title$ C++ client library from a larger
# CMake project.

cmake_minimum_required(VERSION 3.10...3.24)
project(google-cloud-cpp-$library$-quickstart CXX)

find_package(google_cloud_cpp_$library$ REQUIRED)

# MSVC requires some additional code to select the correct runtime library
if (VCPKG_TARGET_TRIPLET MATCHES "-static$$")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$$<$$<CONFIG:Debug>:Debug>")
else ()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$$<$$<CONFIG:Debug>:Debug>DLL")
endif ()

# Define your targets.
add_executable(quickstart quickstart.cc)
target_link_libraries(quickstart google-cloud-cpp::$library_prefix$$library$)
)""";
  google::protobuf::io::OstreamOutputStream output(&os);
  google::protobuf::io::Printer printer(&output, '$');
  printer.Print(variables, kText);
}

void GenerateQuickstartMakefile(
    std::ostream& os, std::map<std::string, std::string> const& variables) {
  auto constexpr kText = R"""(# Copyright $copyright_year$ Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This is a minimal Makefile to show how to use the $title$ C++ client
# for developers who use make(1) as their build system.

# The CXX, CXXFLAGS and CXXLD variables are hard-coded. These values work for
# our tests, but applications would typically make them configurable parameters.
CXX=g++
CXXFLAGS=
CXXLD=$$(CXX)
BIN=.

all: $$(BIN)/quickstart

# Configuration variables to compile and link against the $title$ C++
# client library.
CLIENT_MODULE     := google_cloud_cpp_$library$
CLIENT_CXXFLAGS   := $$(shell pkg-config $$(CLIENT_MODULE) --cflags)
CLIENT_CXXLDFLAGS := $$(shell pkg-config $$(CLIENT_MODULE) --libs-only-L)
CLIENT_LIBS       := $$(shell pkg-config $$(CLIENT_MODULE) --libs-only-l)

$$(BIN)/quickstart: quickstart.cc
)""";
  std::string format = kText;
  format += "\t";
  format +=
      R"""($$(CXXLD) $$(CXXFLAGS) $$(CLIENT_CXXFLAGS) $$(CLIENT_CXXLDFLAGS) -o $$@ $$^ $$(CLIENT_LIBS)
)""";
  google::protobuf::io::OstreamOutputStream output(&os);
  google::protobuf::io::Printer printer(&output, '$');
  printer.Print(variables, format.c_str());
}

void GenerateQuickstartWorkspace(
    std::ostream& os, std::map<std::string, std::string> const& variables,
    std::string const& contents) {
  google::protobuf::io::OstreamOutputStream output(&os);
  google::protobuf::io::Printer printer(&output, '$');
  printer.Print(variables, contents.c_str());
}

void GenerateQuickstartBuild(
    std::ostream& os, std::map<std::string, std::string> const& variables) {
  auto constexpr kText = R"""(# Copyright $copyright_year$ Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

licenses(["notice"])  # Apache 2.0

cc_binary(
    name = "quickstart",
    srcs = [
        "quickstart.cc",
    ],
    deps = [
        "@google_cloud_cpp//:$library_prefix$$library$",
    ],
)
)""";
  google::protobuf::io::OstreamOutputStream output(&os);
  google::protobuf::io::Printer printer(&output, '$');
  printer.Print(variables, kText);
}

void GenerateQuickstartBazelrc(
    std::ostream& os, std::map<std::string, std::string> const& variables) {
  auto constexpr kText = R"""(# Copyright $copyright_year$ Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Use host-OS-specific config lines from bazelrc files.
build --enable_platform_specific_config=true

# The project requires C++ >= 14. By default Bazel adds `-std=c++0x` which
# disables C++14 features, even if the compilers defaults to C++ >= 14
build:linux --cxxopt=-std=c++14
build:macos --cxxopt=-std=c++14
# Protobuf and gRPC require (or soon will require) C++14 to compile the "host"
# targets, such as protoc and the grpc plugin.
build:linux --host_cxxopt=-std=c++14
build:macos --host_cxxopt=-std=c++14

# Do not create the convenience links. They are inconvenient when the build
# runs inside a docker image or if one builds a quickstart and then builds
# the project separately.
build --experimental_convenience_symlinks=ignore
)""";
  google::protobuf::io::OstreamOutputStream output(&os);
  google::protobuf::io::Printer printer(&output, '$');
  printer.Print(variables, kText);
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
