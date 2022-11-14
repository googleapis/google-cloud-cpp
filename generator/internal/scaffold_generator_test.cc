// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "generator/internal/scaffold_generator.h"
#include "google/cloud/internal/random.h"
#include <gmock/gmock.h>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Pair;

void RemoveDirectory(std::string const& path) {
#if _WIN32
  _rmdir(path.c_str());
#else
  rmdir(path.c_str());
#endif  // _WIN32
}

char const* const kHierarchy[] = {
    "/external/",
    "/external/googleapis/",
    "/external/googleapis/protolists/",
    "/external/googleapis/protodeps/",
};

TEST(ScaffoldGeneratorTest, LibraryName) {
  EXPECT_EQ("test", LibraryName("google/cloud/test"));
  EXPECT_EQ("test", LibraryName("google/cloud/test/"));
  EXPECT_EQ("test", LibraryName("google/cloud/test/v1"));
  EXPECT_EQ("golden", LibraryName("blah/golden"));
}

class ScaffoldGenerator : public ::testing::Test {
 protected:
  ~ScaffoldGenerator() override {
    std::remove((path_ + "/api-index-v1.json").c_str());
    std::remove((path_ + "/external/googleapis/protolists/test.list").c_str());
    std::remove((path_ + "/external/googleapis/protodeps/test.deps").c_str());
    // Remove in reverse order, `std::rbegin()` is a C++14 feature.
    auto const size = sizeof(kHierarchy) / sizeof(kHierarchy[0]);
    for (std::size_t i = 0; i != size; ++i) {
      RemoveDirectory(path_ + kHierarchy[size - 1 - i]);
    }
    RemoveDirectory(path_);
  }

  void SetUp() override {
    // Avoid collisions if multiple tests are running in parallel.
    auto generator =
        google::cloud::internal::DefaultPRNG(std::random_device{}());
    auto const directory = google::cloud::internal::Sample(
        generator, 16, "abcdefghijklmnopqrstuvwxyz");
    path_ = ::testing::TempDir() + directory;
    MakeDirectory(path_);
    for (auto const* s : kHierarchy) MakeDirectory(path_ + s);
    std::ofstream(path_ + "/api-index-v1.json") << R"""({
    "apis": [
      {
        "id": "google.cloud.test.v1",
        "directory": "google/cloud/test/v1",
        "version": "v1",
        "majorVersion": "v1",
        "hostName": "test.googleapis.com",
        "title": "Test Only API",
        "description": "Provides a placeholder to write this test."
      }
    ]
})""";

    std::ofstream(path_ + "/external/googleapis/protolists/test.list")
        << R"""(@com_google_googleapis//google/cloud/test/v1:foo.proto
@com_google_googleapis//google/cloud/test/v1:admin.proto
)""";

    std::ofstream(path_ + "/external/googleapis/protodeps/test.deps")
        << R"""(@com_google_googleapis//google/longrunning:operations_proto
@com_google_googleapis//google/rpc:status_proto
@com_google_googleapis//google/api:resource_proto
@com_google_googleapis//google/api:field_behavior_proto
@com_google_googleapis//google/api:client_proto
@com_google_googleapis//google/api:annotations_proto
@com_google_googleapis//google/api:http_proto
)""";

    google::cloud::cpp::generator::ServiceConfiguration service;
    service.set_product_path("google/cloud/test/");
    service.set_service_proto_path("google/cloud/test/v1/service.proto");
    service.set_initial_copyright_year("2034");
    service_ = std::move(service);
  }

  std::string const& path() const { return path_; }
  google::cloud::cpp::generator::ServiceConfiguration const& service() const {
    return service_;
  }

 private:
  std::string path_;
  google::cloud::cpp::generator::ServiceConfiguration service_;
};

TEST_F(ScaffoldGenerator, Vars) {
  auto const ga = ScaffoldVars(path(), service(), false);
  EXPECT_THAT(
      ga,
      AllOf(Contains(Pair("title", "Test Only API")),
            Contains(Pair("description",
                          "Provides a placeholder to write this test.")),
            Contains(Pair("library", "test")),
            Contains(Pair("product_options_page", "google-cloud-test-options")),
            Contains(Pair("copyright_year", "2034")),
            Contains(Pair("library_prefix", "")),
            Contains(Pair("doxygen_version_suffix", "")),
            Contains(Pair("construction", "")),
            Contains(Pair("status", HasSubstr("**GA**")))));

  auto const experimental = ScaffoldVars(path(), service(), true);
  EXPECT_THAT(
      experimental,
      AllOf(Contains(Pair("title", "Test Only API")),
            Contains(Pair("description",
                          "Provides a placeholder to write this test.")),
            Contains(Pair("library", "test")),
            Contains(Pair("product_options_page", "google-cloud-test-options")),
            Contains(Pair("copyright_year", "2034")),
            Contains(Pair("library_prefix", "experimental-")),
            Contains(Pair("doxygen_version_suffix", " (Experimental)")),
            Contains(Pair("construction", "\n:construction:\n")),
            Contains(Pair("status", HasSubstr("**experimental**")))));
}

TEST_F(ScaffoldGenerator, CmakeConfigIn) {
  auto const vars = ScaffoldVars(path(), service(), false);
  std::ostringstream os;
  GenerateCmakeConfigIn(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr("2034"));
  EXPECT_THAT(actual, Not(HasSubstr("$copyright_year$")));
  EXPECT_THAT(
      actual,
      HasSubstr(
          R"""(include("${CMAKE_CURRENT_LIST_DIR}/google_cloud_cpp_test-targets.cmake"))"""));
}

TEST_F(ScaffoldGenerator, Readme) {
  auto const vars = ScaffoldVars(path(), service(), false);
  std::ostringstream os;
  GenerateReadme(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr(R"""(
[cloud-service-docs]: https://cloud.google.com/test
[doxygen-link]: https://googleapis.dev/cpp/google-cloud-test/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/test
)"""));
  EXPECT_THAT(actual, Not(HasSubstr("$construction$")));
  EXPECT_THAT(actual, HasSubstr("**GA**"));
  EXPECT_THAT(actual, Not(HasSubstr("$status$")));
}

TEST_F(ScaffoldGenerator, Build) {
  auto const vars = ScaffoldVars(path(), service(), false);
  std::ostringstream os;
  GenerateBuild(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr("2034"));
  EXPECT_THAT(actual, Not(HasSubstr("$copyright_year$")));
  EXPECT_THAT(actual, HasSubstr(R"""(name = "google_cloud_cpp_test",)"""));
  EXPECT_THAT(
      actual,
      HasSubstr(
          R"""(@com_google_googleapis//google/cloud/test/v1:test_cc_grpc",)"""));
}

TEST_F(ScaffoldGenerator, CMakeLists) {
  auto const vars = ScaffoldVars(path(), service(), false);
  std::ostringstream os;
  GenerateCMakeLists(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr("2034"));
  EXPECT_THAT(actual, Not(HasSubstr("$copyright_year$")));
  EXPECT_THAT(actual, Not(HasSubstr("$library_prefix$")));
  EXPECT_THAT(actual, Not(HasSubstr("$doxygen_version_suffix$")));
  EXPECT_THAT(actual, HasSubstr(R"""(include(CompileProtos)
google_cloud_cpp_load_protolist(
    proto_list
    "${PROJECT_SOURCE_DIR}/external/googleapis/protolists/test.list")
google_cloud_cpp_load_protodeps(
    proto_deps
    "${PROJECT_SOURCE_DIR}/external/googleapis/protodeps/test.deps")
google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_test_protos # cmake-format: sort
    ${proto_list}
    PROTO_PATH_DIRECTORIES
    "${EXTERNAL_GOOGLEAPIS_SOURCE}" "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(test_protos)
target_link_libraries(google_cloud_cpp_test_protos PUBLIC ${proto_deps})
)"""));

  EXPECT_THAT(actual, HasSubstr(R"""(add_executable(test_quickstart)"""));
  EXPECT_THAT(actual, HasSubstr(R"""(EDIT HERE)"""));
}

TEST_F(ScaffoldGenerator, DoxygenMainPage) {
  auto const vars = ScaffoldVars(path(), service(), false);
  std::ostringstream os;
  GenerateDoxygenMainPage(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr(R"""(
An idiomatic C++ client library for the [Test Only API][cloud-service-docs], a service
to Provides a placeholder to write this test.
)"""));
  EXPECT_THAT(actual, HasSubstr(R"""(
[cloud-service-docs]: https://cloud.google.com/test
)"""));
  EXPECT_THAT(actual, Not(HasSubstr("$status$")));
  EXPECT_THAT(actual, HasSubstr("**GA**"));
}

TEST_F(ScaffoldGenerator, DoxygenOptionsPage) {
  auto const vars = ScaffoldVars(path(), service(), false);
  std::ostringstream os;
  GenerateDoxygenOptionsPage(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr(R"""(
@defgroup google-cloud-test-options Test Only API Configuration Options
)"""));
}

TEST_F(ScaffoldGenerator, QuickstartReadme) {
  auto const vars = ScaffoldVars(path(), service(), false);
  std::ostringstream os;
  GenerateQuickstartReadme(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(
      actual,
      HasSubstr(R"""(# HOWTO: using the Test Only API C++ client in your project
)"""));
}

TEST_F(ScaffoldGenerator, QuickstartSkeleton) {
  auto const vars = ScaffoldVars(path(), service(), false);
  std::ostringstream os;
  GenerateQuickstartSkeleton(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr("2034"));
  EXPECT_THAT(actual, Not(HasSubstr("$copyright_year$")));
}

TEST_F(ScaffoldGenerator, QuickstartCMake) {
  auto const vars = ScaffoldVars(path(), service(), false);
  std::ostringstream os;
  GenerateQuickstartCMake(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr("2034"));
  EXPECT_THAT(actual, Not(HasSubstr("$copyright_year$")));
  EXPECT_THAT(actual, Not(HasSubstr("$library_prefix$")));
}

TEST_F(ScaffoldGenerator, QuickstartMakefile) {
  auto const vars = ScaffoldVars(path(), service(), false);
  std::ostringstream os;
  GenerateQuickstartMakefile(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr("2034"));
  EXPECT_THAT(actual, Not(HasSubstr("$copyright_year$")));
  EXPECT_THAT(actual, HasSubstr("\t$(CXXLD) "));
}

TEST_F(ScaffoldGenerator, QuickstartWorkspace) {
  auto const vars = ScaffoldVars(path(), service(), false);
  std::ostringstream os;
  GenerateQuickstartWorkspace(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr("2034"));
  EXPECT_THAT(actual, Not(HasSubstr("$copyright_year$")));
}

TEST_F(ScaffoldGenerator, QuickstartBuild) {
  auto const vars = ScaffoldVars(path(), service(), false);
  std::ostringstream os;
  GenerateQuickstartBuild(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr("2034"));
  EXPECT_THAT(actual, Not(HasSubstr("$copyright_year$")));
  EXPECT_THAT(actual, Not(HasSubstr("$library_prefix$")));
}

TEST_F(ScaffoldGenerator, QuickstartBazelrc) {
  auto const vars = ScaffoldVars(path(), service(), false);
  std::ostringstream os;
  GenerateQuickstartBazelrc(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr("2034"));
  EXPECT_THAT(actual, Not(HasSubstr("$copyright_year$")));
}

TEST_F(ScaffoldGenerator, SamplesBuild) {
  auto const vars = ScaffoldVars(path(), service(), false);
  std::ostringstream os;
  GenerateSamplesBuild(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr("2034"));
  EXPECT_THAT(actual, Not(HasSubstr("$copyright_year$")));
  EXPECT_THAT(actual, Not(HasSubstr("$library_prefix$")));
}

TEST_F(ScaffoldGenerator, SamplesCMake) {
  auto const vars = ScaffoldVars(path(), service(), false);
  std::ostringstream os;
  GenerateSamplesCMake(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr("2034"));
  EXPECT_THAT(actual, HasSubstr("google_cloud_cpp_add_samples(test)"));
  EXPECT_THAT(actual, Not(HasSubstr("$copyright_year$")));
  EXPECT_THAT(actual, Not(HasSubstr("$library_prefix$")));
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
