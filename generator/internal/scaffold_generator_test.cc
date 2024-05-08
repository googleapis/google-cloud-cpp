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
#include "generator/internal/codegen_utils.h"
#include "google/cloud/internal/random.h"
#include <gmock/gmock.h>
#include <cstdlib>
#include <fstream>
#include <sstream>
#if _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif  // _WIN32

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
    "/google/",
    "/google/cloud/",
    "/google/cloud/test/",
    "/google/cloud/test/v1/",
};

TEST(ScaffoldGeneratorTest, LibraryName) {
  EXPECT_EQ("test", LibraryName("google/cloud/test"));
  EXPECT_EQ("test", LibraryName("google/cloud/test/"));
  EXPECT_EQ("test", LibraryName("google/cloud/test/v1"));
  EXPECT_EQ("test", LibraryName("google/cloud/test/v1/"));
  EXPECT_EQ("test", LibraryName("google/cloud/test/foo/v1"));
  EXPECT_EQ("golden", LibraryName("blah/golden"));
  EXPECT_EQ("golden", LibraryName("blah/golden/v1"));
  EXPECT_EQ("service", LibraryName("foo/bar/service"));
}

TEST(ScaffoldGeneratorTest, LibraryPath) {
  EXPECT_EQ("google/cloud/test/", LibraryPath("google/cloud/test"));
  EXPECT_EQ("google/cloud/test/", LibraryPath("google/cloud/test/"));
  EXPECT_EQ("google/cloud/test/", LibraryPath("google/cloud/test/v1"));
  EXPECT_EQ("google/cloud/test/", LibraryPath("google/cloud/test/v1/"));
  EXPECT_EQ("google/cloud/test/", LibraryPath("google/cloud/test/foo/v1"));
  EXPECT_EQ("blah/golden/", LibraryPath("blah/golden"));
  EXPECT_EQ("blah/golden/", LibraryPath("blah/golden/v1"));
  EXPECT_EQ("foo/bar/service/", LibraryPath("foo/bar/service"));
}

TEST(ScaffoldGeneratorTest, ServiceSubdirectory) {
  EXPECT_EQ("", ServiceSubdirectory("google/cloud/test"));
  EXPECT_EQ("", ServiceSubdirectory("google/cloud/test/"));
  EXPECT_EQ("v1/", ServiceSubdirectory("google/cloud/test/v1"));
  EXPECT_EQ("v1/", ServiceSubdirectory("google/cloud/test/v1/"));
  EXPECT_EQ("foo/v1/", ServiceSubdirectory("google/cloud/test/foo/v1"));
  EXPECT_EQ("", ServiceSubdirectory("blah/golden"));
  EXPECT_EQ("v1/", ServiceSubdirectory("blah/golden/v1"));
  EXPECT_EQ("v1/", ServiceSubdirectory("blah/golden/v1"));
  EXPECT_EQ("", ServiceSubdirectory("foo/bar/service"));
}

TEST(ScaffoldGeneratorTest, OptionsGroup) {
  EXPECT_EQ("google-cloud-test-options", OptionsGroup("google/cloud/test"));
  EXPECT_EQ("google-cloud-test-options", OptionsGroup("google/cloud/test/v1"));
  EXPECT_EQ("blah-golden-options", OptionsGroup("blah/golden"));
  EXPECT_EQ("blah-golden-options", OptionsGroup("blah/golden/v1"));
  EXPECT_EQ("foo-bar-service-options", OptionsGroup("foo/bar/service"));
}

nlohmann::json MockIndex() {
  auto api = nlohmann::json{
      {"id", "google.cloud.test.v1"},
      {"directory", "google/cloud/test/v1"},
      {"version", "v1"},
      {"majorversion", "v1"},
      {"hostName", "test.googleapis.com"},
      {"title", "Test Only API"},
      {"configFile", "test_v1.yaml"},
  };
  return nlohmann::json{{"apis", std::vector<nlohmann::json>{api}}};
}

class ScaffoldGenerator : public ::testing::Test {
 protected:
  ~ScaffoldGenerator() override {
    std::remove((path_ + "/api-index-v1.json").c_str());
    std::remove((path_ + "/external/googleapis/protolists/test.list").c_str());
    std::remove((path_ + "/external/googleapis/protodeps/test.deps").c_str());
    std::remove((path_ + "/google/cloud/test/v1/test_v1.yaml").c_str());
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
    std::ofstream(path_ + "/api-index-v1.json") << MockIndex().dump(4) << "\n";

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

    std::ofstream(path_ + "/google/cloud/test/v1/test_v1.yaml")
        << R"""(type: google.api.Service
config_version: 1
name: test.googleapis.com
title: Test API

publishing:
  new_issue_uri: https://issuetracker.google.com/issues/new?component=8675309
  documentation_uri: https://cloud.google.com/test/docs
  api_short_name: test
)""";

    google::cloud::cpp::generator::ServiceConfiguration service;
    service.set_product_path("google/cloud/test/v1");
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
  auto const ga = ScaffoldVars(path(), MockIndex(), service(), false);
  EXPECT_THAT(
      ga,
      AllOf(Contains(Pair("title", "Test Only API")),
            Contains(Pair("library", "test")),
            Contains(Pair("service_subdirectory", "v1/")),
            Contains(Pair("product_options_page", "google-cloud-test-options")),
            Contains(Pair("copyright_year", "2034")),
            Contains(Pair("library_prefix", "")),
            Contains(Pair("experimental", "")),
            Contains(Pair("construction", "")),
            Contains(Pair("status", HasSubstr("**GA**")))));

  auto const experimental = ScaffoldVars(path(), MockIndex(), service(), true);
  EXPECT_THAT(
      experimental,
      AllOf(Contains(Pair("title", "Test Only API")),
            Contains(Pair("library", "test")),
            Contains(Pair("service_subdirectory", "v1/")),
            Contains(Pair("product_options_page", "google-cloud-test-options")),
            Contains(Pair("copyright_year", "2034")),
            Contains(Pair("library_prefix", "experimental-")),
            Contains(Pair("experimental", " EXPERIMENTAL")),
            Contains(Pair("construction", "\n:construction:\n")),
            Contains(Pair("status", HasSubstr("**experimental**")))));
}

TEST_F(ScaffoldGenerator, Readme) {
  auto const vars = ScaffoldVars(path(), MockIndex(), service(), false);
  std::ostringstream os;
  GenerateReadme(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr(R"""(
[cloud-service-docs]: https://cloud.google.com/test/docs
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/test/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/test
)"""));
  EXPECT_THAT(actual, Not(HasSubstr("$construction$")));
  EXPECT_THAT(actual, HasSubstr("**GA**"));
  EXPECT_THAT(actual, Not(HasSubstr("$status$")));
}

TEST_F(ScaffoldGenerator, ReadmeNoPublishingDocumentationUri) {
  std::ofstream(path() + "/google/cloud/test/v1/test_v1.yaml")
      << R"""(type: google.api.Service
config_version: 1
name: test.googleapis.com
title: Test API

publishing:
  new_issue_uri: https://issuetracker.google.com/issues/new?component=8675309
  api_short_name: test
)""";

  auto const vars = ScaffoldVars(path(), MockIndex(), service(), false);
  std::ostringstream os;
  GenerateReadme(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr(R"""(
[cloud-service-docs]: https://cloud.google.com/test [EDIT HERE]
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/test/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/test
)"""));
  EXPECT_THAT(actual, Not(HasSubstr("$construction$")));
  EXPECT_THAT(actual, HasSubstr("**GA**"));
  EXPECT_THAT(actual, Not(HasSubstr("$status$")));
}

TEST_F(ScaffoldGenerator, ReadmeNoPublishing) {
  std::ofstream(path() + "/google/cloud/test/v1/test_v1.yaml")
      << R"""(type: google.api.Service
config_version: 1
name: test.googleapis.com
title: Test API
)""";

  auto const vars = ScaffoldVars(path(), MockIndex(), service(), false);
  std::ostringstream os;
  GenerateReadme(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr(R"""(
[cloud-service-docs]: https://cloud.google.com/test [EDIT HERE]
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/test/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/test
)"""));
  EXPECT_THAT(actual, Not(HasSubstr("$construction$")));
  EXPECT_THAT(actual, HasSubstr("**GA**"));
  EXPECT_THAT(actual, Not(HasSubstr("$status$")));
}

TEST_F(ScaffoldGenerator, Build) {
  auto const vars = ScaffoldVars(path(), MockIndex(), service(), false);
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
  auto const vars = ScaffoldVars(path(), MockIndex(), service(), false);
  std::ostringstream os;
  GenerateCMakeLists(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr("2034"));
  EXPECT_THAT(actual, Not(HasSubstr("$copyright_year$")));
  EXPECT_THAT(actual, Not(HasSubstr("$library_prefix$")));
  EXPECT_THAT(actual, Not(HasSubstr("$experimental$")));
  EXPECT_THAT(actual, HasSubstr(R"""(include(GoogleCloudCppLibrary)

google_cloud_cpp_add_gapic_library(test "Test Only API"
    SERVICE_DIRS "v1/")
)"""));

  EXPECT_THAT(actual, HasSubstr(R"""(add_executable(test_quickstart)"""));
  EXPECT_THAT(actual, HasSubstr(R"""(EDIT HERE)"""));
}

TEST_F(ScaffoldGenerator, DoxygenMainPage) {
  auto const vars = ScaffoldVars(path(), MockIndex(), service(), false);
  std::ostringstream os;
  GenerateDoxygenMainPage(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr(R"""(
An idiomatic C++ client library for the [Test Only API][cloud-service-docs].
)"""));
  EXPECT_THAT(actual, HasSubstr(R"""(
[cloud-service-docs]: https://cloud.google.com/test/docs
)"""));
  EXPECT_THAT(actual, Not(HasSubstr("$status$")));
  EXPECT_THAT(actual, HasSubstr("**GA**"));
  EXPECT_THAT(actual, HasSubstr(R"""(
## More Information

- @ref common-error-handling - describes how the library reports errors.
- @ref test-override-endpoint - describes how to override the default
  endpoint.
- @ref test-override-authentication - describes how to change the
  authentication credentials used by the library.
- @ref test-override-retry - describes how to change the default retry
  policies.
)"""));
}

TEST_F(ScaffoldGenerator, DoxygenOptionsPage) {
  auto const vars = ScaffoldVars(path(), MockIndex(), service(), false);
  std::ostringstream os;
  GenerateDoxygenOptionsPage(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr(R"""(
@defgroup google-cloud-test-options Test Only API Configuration Options
)"""));
}

TEST_F(ScaffoldGenerator, DoxygenEnvironmentPage) {
  auto const vars = ScaffoldVars(path(), MockIndex(), service(), false);
  std::ostringstream os;
  GenerateDoxygenEnvironmentPage(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, AllOf(HasSubstr(R"""(
@page test-env Environment Variables
)"""),
                            HasSubstr(R"""(
@section test-env-logging Logging
)"""),
                            HasSubstr(R"""(
@section test-env-endpoint Endpoint Overrides
)"""),
                            HasSubstr(R"""(
@section test-env-project Setting the Default Project
)""")));
}

TEST_F(ScaffoldGenerator, OverrideAuthenticationPage) {
  auto const vars = ScaffoldVars(path(), MockIndex(), service(), false);
  std::ostringstream os;
  GenerateOverrideAuthenticationPage(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, AllOf(HasSubstr(R"""(
@page test-override-authentication How to Override the Authentication Credentials
)""")));
}

TEST_F(ScaffoldGenerator, OverrideEndpointPage) {
  auto const vars = ScaffoldVars(path(), MockIndex(), service(), false);
  std::ostringstream os;
  GenerateOverrideEndpointPage(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, AllOf(HasSubstr(R"""(
@page test-override-endpoint How to Override the Default Endpoint
)""")));
}

TEST_F(ScaffoldGenerator, OverrideRetryPoliciesPage) {
  auto const vars = ScaffoldVars(path(), MockIndex(), service(), false);
  std::ostringstream os;
  GenerateOverrideRetryPoliciesPage(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, AllOf(HasSubstr(R"""(
@page test-override-retry Override Retry, Backoff, and Idempotency Policies
)""")));
}

TEST_F(ScaffoldGenerator, QuickstartReadme) {
  auto const vars = ScaffoldVars(path(), MockIndex(), service(), false);
  std::ostringstream os;
  GenerateQuickstartReadme(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(
      actual,
      HasSubstr(R"""(# HOWTO: using the Test Only API C++ client in your project
)"""));
}

TEST_F(ScaffoldGenerator, QuickstartSkeleton) {
  auto const vars = ScaffoldVars(path(), MockIndex(), service(), false);
  std::ostringstream os;
  GenerateQuickstartSkeleton(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr("2034"));
  EXPECT_THAT(actual, Not(HasSubstr("$copyright_year$")));
}

TEST_F(ScaffoldGenerator, QuickstartCMake) {
  auto const vars = ScaffoldVars(path(), MockIndex(), service(), false);
  std::ostringstream os;
  GenerateQuickstartCMake(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr("2034"));
  EXPECT_THAT(actual, Not(HasSubstr("$copyright_year$")));
  EXPECT_THAT(actual, Not(HasSubstr("$library_prefix$")));
}

TEST_F(ScaffoldGenerator, QuickstartMakefile) {
  auto const vars = ScaffoldVars(path(), MockIndex(), service(), false);
  std::ostringstream os;
  GenerateQuickstartMakefile(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr("2034"));
  EXPECT_THAT(actual, Not(HasSubstr("$copyright_year$")));
  EXPECT_THAT(actual, HasSubstr("\t$(CXXLD) "));
}

TEST_F(ScaffoldGenerator, QuickstartWorkspace) {
  auto constexpr kContents = R"""(# Copyright $copyright_year$ Google LLC

# A minimal WORKSPACE file showing how to use the $title$
# C++ client library in Bazel-based projects.
workspace(name = "qs")
)""";

  auto const vars = ScaffoldVars(path(), MockIndex(), service(), false);
  std::ostringstream os;
  GenerateQuickstartWorkspace(os, vars, kContents);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr("# Copyright 2034 Google LLC"));
  EXPECT_THAT(actual, Not(HasSubstr("$copyright_year$")));
  EXPECT_THAT(
      actual,
      HasSubstr(
          "A minimal WORKSPACE file showing how to use the Test Only API"));
  EXPECT_THAT(actual, Not(HasSubstr("$title$")));
}

TEST_F(ScaffoldGenerator, QuickstartBuild) {
  auto const vars = ScaffoldVars(path(), MockIndex(), service(), false);
  std::ostringstream os;
  GenerateQuickstartBuild(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr("2034"));
  EXPECT_THAT(actual, Not(HasSubstr("$copyright_year$")));
  EXPECT_THAT(actual, Not(HasSubstr("$library_prefix$")));
}

TEST_F(ScaffoldGenerator, QuickstartBazelrc) {
  auto const vars = ScaffoldVars(path(), MockIndex(), service(), false);
  std::ostringstream os;
  GenerateQuickstartBazelrc(os, vars);
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr("2034"));
  EXPECT_THAT(actual, Not(HasSubstr("$copyright_year$")));
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
