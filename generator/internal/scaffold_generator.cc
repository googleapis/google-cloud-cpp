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
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/filesystem.h"
#include "google/cloud/log.h"
#include "absl/strings/str_split.h"
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iterator>

namespace google {
namespace cloud {
namespace generator_internal {

auto constexpr kApiIndexFilename = "api-index-v1.json";

std::string LibraryName(
    google::cloud::cpp::generator::ServiceConfiguration const& service) {
  auto const l = service.product_path().find_last_of('/');
  if (l == std::string::npos) return {};
  return service.product_path().substr(l + 1);
}

std::string SiteRoot(
    google::cloud::cpp::generator::ServiceConfiguration const& service) {
  // TODO(#7605) - get a configurable source for this
  return LibraryName(service);
}

std::vector<std::string> ProtoList(std::string const& project_root,
                                   std::string const& library) {
  auto const filename =
      project_root + "/external/googleapis/protolists/" + library + ".list";
  auto status = google::cloud::internal::status(filename);
  if (!exists(status)) {
    std::cout << "Cannot find " << filename << "\n";
    return {};
  }
  std::ifstream is(filename);
  return absl::StrSplit(
      std::string{std::istreambuf_iterator<char>(is.rdbuf()), {}}, "\n");
}

std::vector<std::string> ProtoDependencies(std::string const& project_root,
                                           std::string const& library) {
  auto const filename =
      project_root + "/external/googleapis/protodeps/" + library + ".deps";
  auto status = google::cloud::internal::status(filename);
  if (!exists(status)) return {};
  std::ifstream is(filename);
  return absl::StrSplit(
      std::string{std::istreambuf_iterator<char>(is.rdbuf()), {}}, "\n");
}

std::map<std::string, std::string> ScaffoldVars(
    std::string const& googleapis_path, std::string const& project_root,
    google::cloud::cpp::generator::ServiceConfiguration const& service) {
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
    return {};
  }
  if (!index.contains("apis")) {
    GCP_LOG(WARNING) << "Missing `apis` field in API index file ("
                     << api_index_path << ")";
    return {};
  }
  std::map<std::string, std::string> vars;
  for (auto const& api : index["apis"]) {
    if (!api.contains("directory")) continue;
    auto const directory = api["directory"].get<std::string>() + "/";
    if (service.service_proto_path().rfind(directory, 0) != 0) continue;
    vars.emplace("title", api.value("title", ""));
    vars.emplace("description", api.value("description", ""));
    vars.emplace("directory", api.value("directory", ""));
  }
  auto const library = LibraryName(service);
  vars["copyright_year"] = service.initial_copyright_year();
  vars["library"] = library;
  vars["site_root"] = SiteRoot(service);

  auto const proto_list = ProtoList(project_root, library);
  std::vector<std::string> protos(proto_list.size());
  std::transform(proto_list.begin(), proto_list.end(), protos.begin(),
                 [](std::string x) {
                   x.erase(0, std::strlen("@com_google_googleapis//"));
                   std::replace(x.begin(), x.end(), ':', '/');
                   if (x.empty()) return x;
                   return "${EXTERNAL_GOOGLEAPIS_SOURCE}/" + x;
                 });
  auto end = std::remove_if(protos.begin(), protos.end(),
                            [](std::string const& x) { return x.empty(); });
  vars["proto_files"] = absl::StrJoin(protos.begin(), end, "\n    ");

  auto const dep_list = ProtoDependencies(project_root, library);
  std::vector<std::string> deps(dep_list.size());
  std::transform(
      dep_list.begin(), dep_list.end(), deps.begin(), [](std::string x) {
        x.erase(0, std::strlen("@com_google_googleapis//"));
        std::replace(x.begin(), x.end(), ':', '_');
        std::replace(x.begin(), x.end(), '/', '_');
        if (x.rfind("google_", 0) == 0) {
          x = "google-cloud-cpp::" + x.substr(std::strlen("google_"));
        }
        if (x.empty()) return x;
        // Our libraries end with _protos, the Bazel libraries end
        // with _proto.
        x += "s";
        return x;
      });
  end = std::remove_if(deps.begin(), deps.end(),
                       [](std::string const& x) { return x.empty(); });
  vars["proto_deps"] = absl::StrJoin(deps.begin(), end, "\n    ");

  vars["cpp_files"] = "# EDIT HERE: add list of C++ files\n";
  vars["mock_files"] = "# EDIT HERE: add list of mock headers\n";

  return vars;
}

void GenerateScaffold(
    std::string const& googleapis_path, std::string const& output_path,
    google::cloud::cpp::generator::ServiceConfiguration const& service) {
  using Generator = std::function<void(
      std::ostream&, std::map<std::string, std::string> const&)>;
  struct Destination {
    std::string name;
    Generator generator;
  } files[] = {
      {"config.cmake.in", GenerateCmakeConfigIn},
      {"config.pc.in", GenerateConfigPcIn},
      {"README.md", GenerateReadme},
      {"BUILD", GenerateBuild},
      {"CMakeLists.txt", GenerateCMakeLists},
  };

  auto const vars = ScaffoldVars(googleapis_path, output_path, service);
  auto const destination = output_path + "/" + service.product_path() + "/";
  for (auto const& f : files) {
    std::ofstream os(destination + f.name);
    f.generator(os, vars);
  }
}

void GenerateCmakeConfigIn(
    std::ostream& os, std::map<std::string, std::string> const& variables) {
  auto constexpr kText =
      R"""(# Copyright $copyright_year$ Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

include(CMakeFindDependencyMacro)
# google_cloud_cpp_googleapis finds both gRPC and Protobuf, no need to load them here.
find_dependency(google_cloud_cpp_googleapis)
find_dependency(google_cloud_cpp_common)
find_dependency(google_cloud_cpp_grpc_utils)
find_dependency(absl)

include("$${CMAKE_CURRENT_LIST_DIR}/google_cloud_cpp_$library$-targets.cmake")
)""";
  google::protobuf::io::OstreamOutputStream output(&os);
  google::protobuf::io::Printer printer(&output, '$');
  printer.Print(variables, kText);
}

void GenerateConfigPcIn(std::ostream& os,
                        std::map<std::string, std::string> const& variables) {
  auto constexpr kText = R"""(# Copyright $copyright_year$ Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

prefix=$${pcfiledir}/../..
exec_prefix=$${prefix}/@CMAKE_INSTALL_BINDIR@
libdir=$${prefix}/@CMAKE_INSTALL_LIBDIR@
includedir=$${prefix}/@CMAKE_INSTALL_INCLUDEDIR@

Name: @GOOGLE_CLOUD_PC_NAME@
Description: @GOOGLE_CLOUD_PC_DESCRIPTION@
Requires: @GOOGLE_CLOUD_PC_REQUIRES@
Version: @DOXYGEN_PROJECT_NUMBER@

Libs: -L$${libdir} @GOOGLE_CLOUD_PC_LIBS@
Cflags: -I$${includedir}
)""";
  google::protobuf::io::OstreamOutputStream output(&os);
  google::protobuf::io::Printer printer(&output, '$');
  printer.Print(variables, kText);
}

void GenerateReadme(std::ostream& os,
                    std::map<std::string, std::string> const& variables) {
  auto constexpr kText = R"""(# $title$ C++ Client Library

:construction:

This directory contains an idiomatic C++ client library for
[$title$](https://cloud.google.com/$site_root$/), a service that
$description$

This library is **experimental**. Its APIS are subject to change without notice.
)""";
  google::protobuf::io::OstreamOutputStream output(&os);
  google::protobuf::io::Printer printer(&output, '$');
  printer.Print(variables, kText);
}

void GenerateBuild(std::ostream& os,
                   std::map<std::string, std::string> const& variables) {
  auto constexpr kText = R"""(# Copyright $copyright_year$ Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

package(default_visibility = ["//visibility:private"])

licenses(["notice"])  # Apache 2.0

load(":google_cloud_cpp_$library$.bzl", "google_cloud_cpp_$library$_hdrs", "google_cloud_cpp_$library$_srcs")

cc_library(
    name = "google_cloud_cpp_$library$",
    srcs = google_cloud_cpp_$library$_srcs,
    hdrs = google_cloud_cpp_$library$_hdrs,
    visibility = ["//:__pkg__"],
    deps = [
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_grpc_utils",
        "@com_google_googleapis//$directory$:$library$_cc_grpc",
    ],
)

load(":google_cloud_cpp_$library$_mocks.bzl", "google_cloud_cpp_$library$_mocks_hdrs", "google_cloud_cpp_$library$_mocks_srcs")

cc_library(
    name = "google_cloud_cpp_$library$_mocks",
    srcs = google_cloud_cpp_$library$_mocks_srcs,
    hdrs = google_cloud_cpp_$library$_mocks_hdrs,
    visibility = ["//:__pkg__"],
    deps = [
        ":google_cloud_cpp_$library$",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_grpc_utils",
    ],
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
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ~~~

include(GoogleapisConfig)
set(DOXYGEN_PROJECT_NAME "$title$ C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for the $title$")
set(DOXYGEN_PROJECT_NUMBER "$${PROJECT_VERSION} (Experimental)")
set(DOXYGEN_EXCLUDE_SYMBOLS "internal" "$library$_internal" "$library$_testing"
                            "examples")

# Creates the proto headers needed by doxygen.
set(GOOGLE_CLOUD_CPP_DOXYGEN_DEPS google-cloud-cpp::$library$_protos)

find_package(gRPC REQUIRED)
find_package(ProtobufWithTargets REQUIRED)
find_package(absl CONFIG REQUIRED)

include(GoogleCloudCppCommon)

set(EXTERNAL_GOOGLEAPIS_SOURCE
    "$${PROJECT_BINARY_DIR}/external/googleapis/src/googleapis_download")
find_path(PROTO_INCLUDE_DIR google/protobuf/descriptor.proto)
if (PROTO_INCLUDE_DIR)
    list(INSERT PROTOBUF_IMPORT_DIRS 0 "$${PROTO_INCLUDE_DIR}")
endif ()

include(CompileProtos)
google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_$library$_protos # cmake-format: sort
    $proto_files$
    PROTO_PATH_DIRECTORIES
    "$${EXTERNAL_GOOGLEAPIS_SOURCE}" "$${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias($library$_protos)
target_link_libraries(google_cloud_cpp_$library$_protos PUBLIC #
    $proto_deps$)

add_library(google_cloud_cpp_$library$ # cmake-format: sort
            $cpp_files$)
target_include_directories(
    google_cloud_cpp_$library$
    PUBLIC $$<BUILD_INTERFACE:$${PROJECT_SOURCE_DIR}>
           $$<BUILD_INTERFACE:$${PROJECT_BINARY_DIR}>
           $$<INSTALL_INTERFACE:include>)
target_link_libraries(
    google_cloud_cpp_$library$
    PUBLIC google-cloud-cpp::grpc_utils google-cloud-cpp::common
           google-cloud-cpp::$library$_protos)
google_cloud_cpp_add_common_options(google_cloud_cpp_$library$)
set_target_properties(
    google_cloud_cpp_$library$
    PROPERTIES EXPORT_NAME google-cloud-cpp::experimental-$library$
               VERSION "$${PROJECT_VERSION}"
               SOVERSION "$${PROJECT_VERSION_MAJOR}")
target_compile_options(google_cloud_cpp_$library$
                       PUBLIC $${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

add_library(google-cloud-cpp::experimental-$library$ ALIAS google_cloud_cpp_$library$)

# To avoid maintaining the list of files for the library, export them to a .bzl
# file.
include(CreateBazelConfig)
create_bazel_config(google_cloud_cpp_$library$ YEAR "2021")

# Create a header-only library for the mocks. We use a CMake `INTERFACE` library
# for these, a regular library would not work on macOS (where the library needs
# at least one .o file). Unfortunately INTERFACE libraries are a bit weird in
# that they need absolute paths for their sources.
add_library(google_cloud_cpp_$library$_mocks INTERFACE)
target_sources(google_cloud_cpp_$library$_mocks INTERFACE # cmake-format: sort
               $mock_files$)
target_link_libraries(
    google_cloud_cpp_$library$_mocks
    INTERFACE google-cloud-cpp::experimental-$library$ GTest::gmock_main
              GTest::gmock GTest::gtest)
create_bazel_config(google_cloud_cpp_$library$_mocks YEAR "$copyright_year$")
target_include_directories(
    google_cloud_cpp_$library$_mocks
    INTERFACE $$<BUILD_INTERFACE:$${PROJECT_SOURCE_DIR}>
              $$<BUILD_INTERFACE:$${PROJECT_BINARY_DIR}>
              $$<INSTALL_INTERFACE:include>)
target_compile_options(google_cloud_cpp_$library$_mocks
                       INTERFACE $${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

# Get the destination directories based on the GNU recommendations.
include(GNUInstallDirs)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT google_cloud_cpp_$library$-targets
    DESTINATION "$${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_$library$"
    COMPONENT google_cloud_cpp_development)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS google_cloud_cpp_$library$ google_cloud_cpp_$library$_protos
    EXPORT google_cloud_cpp_$library$-targets
    RUNTIME DESTINATION $${CMAKE_INSTALL_BINDIR}
            COMPONENT google_cloud_cpp_runtime
    LIBRARY DESTINATION $${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_runtime
            NAMELINK_SKIP
    ARCHIVE DESTINATION $${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)
# With CMake-3.12 and higher we could avoid this separate command (and the
# duplication).
install(
    TARGETS google_cloud_cpp_$library$ google_cloud_cpp_$library$_protos
    LIBRARY DESTINATION $${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development
            NAMELINK_ONLY
    ARCHIVE DESTINATION $${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_proto_library_protos("google_cloud_cpp_$library$_protos"
                                              "$${EXTERNAL_GOOGLEAPIS_SOURCE}")
google_cloud_cpp_install_proto_library_headers("google_cloud_cpp_$library$_protos")
google_cloud_cpp_install_headers("google_cloud_cpp_$library$"
                                 "include/google/cloud/$library$")
google_cloud_cpp_install_headers("google_cloud_cpp_$library$_mocks"
                                 "include/google/cloud/$library$")

# Setup global variables used in the following *.in files.
set(GOOGLE_CLOUD_CONFIG_VERSION_MAJOR $${PROJECT_VERSION_MAJOR})
set(GOOGLE_CLOUD_CONFIG_VERSION_MINOR $${PROJECT_VERSION_MINOR})
set(GOOGLE_CLOUD_CONFIG_VERSION_PATCH $${PROJECT_VERSION_PATCH})
set(GOOGLE_CLOUD_PC_NAME "The $title$ C++ Client Library")
set(GOOGLE_CLOUD_PC_DESCRIPTION "Provides C++ APIs to access $title$.")
set(GOOGLE_CLOUD_PC_LIBS "-lgoogle_cloud_cpp_$library$")
string(CONCAT GOOGLE_CLOUD_PC_REQUIRES "google_cloud_cpp_grpc_utils"
              " google_cloud_cpp_common" " google_cloud_cpp_$library$_protos")

# Create and install the pkg-config files.
configure_file("$${PROJECT_SOURCE_DIR}/google/cloud/$library$/config.pc.in"
               "google_cloud_cpp_$library$.pc" @ONLY)
install(
    FILES "$${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_$library$.pc"
    DESTINATION "$${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    COMPONENT google_cloud_cpp_development)

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file("config.cmake.in" "google_cloud_cpp_$library$-config.cmake" @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_$library$-config-version.cmake"
    VERSION $${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "$${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_$library$-config.cmake"
        "$${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_$library$-config-version.cmake"
    DESTINATION "$${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_$library$"
    COMPONENT google_cloud_cpp_development)

external_googleapis_install_pc("google_cloud_cpp_$library$_protos"
                               "$${CMAKE_CURRENT_LIST_DIR}")
)""";
  google::protobuf::io::OstreamOutputStream output(&os);
  google::protobuf::io::Printer printer(&output, '$');
  printer.Print(variables, kText);
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
