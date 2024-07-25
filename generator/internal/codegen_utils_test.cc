// Copyright 2020 Google LLC
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

#include "generator/internal/codegen_utils.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ContainsRegex;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Pair;
using ::testing::Values;

TEST(LocalInclude, Success) {
  EXPECT_EQ("#include \"google/cloud/status.h\"\n",
            LocalInclude("google/cloud/status.h"));
}

TEST(LocalInclude, Empty) {
  std::string const empty;
  EXPECT_EQ(empty, LocalInclude(""));
}

TEST(SystemInclude, Success) {
  EXPECT_EQ("#include <vector>\n", SystemInclude("vector"));
}

TEST(SystemInclude, Empty) {
  std::string const empty;
  EXPECT_EQ(empty, SystemInclude(""));
}

TEST(CamelCaseToSnakeCase, Success) {
  EXPECT_EQ("foo_bar_b", CamelCaseToSnakeCase("FooBarB"));
  EXPECT_EQ("foo_bar_baz", CamelCaseToSnakeCase("FooBarBaz"));
  EXPECT_EQ("foo_bar_baz", CamelCaseToSnakeCase("fooBarBaz"));
  EXPECT_EQ("foo_bar_ab", CamelCaseToSnakeCase("fooBarAb"));
  EXPECT_EQ("foo_bar_baaaaa", CamelCaseToSnakeCase("fooBarBAAAAA"));
  EXPECT_EQ("foo_bar_b", CamelCaseToSnakeCase("foo_BarB"));
  EXPECT_EQ("v1", CamelCaseToSnakeCase("v1"));
  EXPECT_EQ("", CamelCaseToSnakeCase(""));
  EXPECT_EQ(" ", CamelCaseToSnakeCase(" "));
  EXPECT_EQ("a", CamelCaseToSnakeCase("A"));
  EXPECT_EQ("a_b", CamelCaseToSnakeCase("aB"));
  EXPECT_EQ("foo123", CamelCaseToSnakeCase("Foo123"));
}

TEST(CamelCaseToSnakeCase, Exceptions) {
  EXPECT_EQ("bigquery_read", CamelCaseToSnakeCase("BigQueryRead"));
}

TEST(ServiceNameToFilePath, TrailingServiceInLastComponent) {
  EXPECT_EQ("google/spanner/admin/database/v1/database_admin",
            ServiceNameToFilePath(
                "google.spanner.admin.database.v1.DatabaseAdminService"));
}

TEST(ServiceNameToFilePath, NoTrailingServiceInLastComponent) {
  EXPECT_EQ(
      "google/spanner/admin/database/v1/database_admin",
      ServiceNameToFilePath("google.spanner.admin.database.v1.DatabaseAdmin"));
}

TEST(ServiceNameToFilePath, TrailingServiceInIntermediateComponent) {
  EXPECT_EQ(
      "google/spanner/admin/database_service/v1/database_admin",
      ServiceNameToFilePath(
          "google.spanner.admin.databaseService.v1.DatabaseAdminService"));
}

TEST(ProtoNameToCppName, MessageType) {
  EXPECT_EQ("google::spanner::admin::database::v1::Request",
            ProtoNameToCppName("google.spanner.admin.database.v1.Request"));
}

TEST(Namespace, Normal) {
  EXPECT_EQ("test", Namespace("google/cloud/test"));
  EXPECT_EQ("test", Namespace("google/cloud/test/"));
  EXPECT_EQ("test_v1", Namespace("google/cloud/test/v1"));
  EXPECT_EQ("test_v1", Namespace("google/cloud/test/v1/"));
  EXPECT_EQ("test_foo_v1", Namespace("google/cloud/test/foo/v1"));
  EXPECT_EQ("golden", Namespace("blah/golden"));
  EXPECT_EQ("golden_v1", Namespace("blah/golden/v1"));
  EXPECT_EQ("service", Namespace("foo/bar/service"));
}

TEST(Namespace, Internal) {
  EXPECT_EQ("test_internal",
            Namespace("google/cloud/test", NamespaceType::kInternal));
  EXPECT_EQ("test_internal",
            Namespace("google/cloud/test/", NamespaceType::kInternal));
  EXPECT_EQ("test_v1_internal",
            Namespace("google/cloud/test/v1", NamespaceType::kInternal));
  EXPECT_EQ("test_v1_internal",
            Namespace("google/cloud/test/v1/", NamespaceType::kInternal));
  EXPECT_EQ("test_foo_v1_internal",
            Namespace("google/cloud/test/foo/v1", NamespaceType::kInternal));
  EXPECT_EQ("golden_internal",
            Namespace("blah/golden", NamespaceType::kInternal));
  EXPECT_EQ("golden_v1_internal",
            Namespace("blah/golden/v1", NamespaceType::kInternal));
  EXPECT_EQ("service_internal",
            Namespace("foo/bar/service", NamespaceType::kInternal));
}

TEST(Namespace, Mocks) {
  EXPECT_EQ("test_mocks",
            Namespace("google/cloud/test", NamespaceType::kMocks));
  EXPECT_EQ("test_mocks",
            Namespace("google/cloud/test/", NamespaceType::kMocks));
  EXPECT_EQ("test_v1_mocks",
            Namespace("google/cloud/test/v1", NamespaceType::kMocks));
  EXPECT_EQ("test_v1_mocks",
            Namespace("google/cloud/test/v1/", NamespaceType::kMocks));
  EXPECT_EQ("test_foo_v1_mocks",
            Namespace("google/cloud/test/foo/v1", NamespaceType::kMocks));
  EXPECT_EQ("golden_mocks", Namespace("blah/golden", NamespaceType::kMocks));
  EXPECT_EQ("golden_v1_mocks",
            Namespace("blah/golden/v1", NamespaceType::kMocks));
  EXPECT_EQ("service_mocks",
            Namespace("foo/bar/service", NamespaceType::kMocks));
}

TEST(ProcessCommandLineArgs, NoProductPath) {
  auto result = ProcessCommandLineArgs("");
  EXPECT_THAT(
      result,
      StatusIs(StatusCode::kInvalidArgument,
               "--cpp_codegen_opt=product_path=<path> must be specified."));
}

TEST(ProcessCommandLineArgs, EmptyProductPath) {
  auto result = ProcessCommandLineArgs("product_path=");
  EXPECT_THAT(
      result,
      StatusIs(StatusCode::kInvalidArgument,
               "--cpp_codegen_opt=product_path=<path> must be specified."));
}

TEST(ProcessCommandLineArgs, ProductPathNeedsFormatting) {
  auto result = ProcessCommandLineArgs("product_path=/google/cloud/pubsub");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, Contains(Pair("product_path", "google/cloud/pubsub/")));
}

TEST(ProcessCommandLineArgs, ProductPathAlreadyFormatted) {
  auto result = ProcessCommandLineArgs("product_path=google/cloud/pubsub/");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, Contains(Pair("product_path", "google/cloud/pubsub/")));
}

TEST(ProcessCommandLineArgs, NoCopyrightYearParameterOrValue) {
  auto result = ProcessCommandLineArgs("product_path=google/cloud/pubsub/");
  auto expected_year = CurrentCopyrightYear();
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, Contains(Pair("copyright_year", expected_year)));
}

TEST(ProcessCommandLineArgs, NoCopyrightYearValue) {
  auto result = ProcessCommandLineArgs(
      "product_path=google/cloud/pubsub/"
      ",copyright_year=");
  auto expected_year = CurrentCopyrightYear();
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, Contains(Pair("copyright_year", expected_year)));
}

TEST(ProcessCommandLineArgs, CopyrightYearWithValue) {
  auto result = ProcessCommandLineArgs(
      "product_path=google/cloud/pubsub/"
      ",copyright_year=1995");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, Contains(Pair("copyright_year", "1995")));
}

TEST(ProcessCommandLineArgs, ServiceEndpointEnvVar) {
  auto result = ProcessCommandLineArgs(
      "product_path=google/cloud/spanner/"
      ",service_endpoint_env_var=GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_ENDPOINT");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result,
              Contains(Pair("service_endpoint_env_var",
                            "GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_ENDPOINT")));
  EXPECT_THAT(*result, Contains(Pair("emulator_endpoint_env_var", "")));
}

TEST(ProcessCommandLineArgs, EmulatorEndpointEnvVar) {
  auto result = ProcessCommandLineArgs(
      "product_path=google/cloud/spanner/"
      ",emulator_endpoint_env_var=SPANNER_EMULATOR_HOST");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, Contains(Pair("emulator_endpoint_env_var",
                                     "SPANNER_EMULATOR_HOST")));
  EXPECT_THAT(*result, Contains(Pair("service_endpoint_env_var", "")));
}

TEST(ProcessCommandLineArgs, ProcessArgOmitService) {
  auto result = ProcessCommandLineArgs(
      "product_path=google/cloud/spanner/"
      ",omit_service=Omitted1"
      ",omit_service=Omitted2");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result,
              Contains(Pair("omitted_services", AllOf(HasSubstr("Omitted1"),
                                                      HasSubstr("Omitted2")))));
}

TEST(ProcessCommandLineArgs, ProcessArgOmitRpc) {
  auto result = ProcessCommandLineArgs(
      "product_path=google/cloud/spanner/"
      ",emulator_endpoint_env_var=SPANNER_EMULATOR_HOST"
      ",omit_rpc=Omitted1"
      ",omit_rpc=Omitted2");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result,
              Contains(Pair("omitted_rpcs", AllOf(HasSubstr("Omitted1"),
                                                  HasSubstr("Omitted2")))));
}

TEST(ProcessCommandLineArgs, ProcessArgGenAsyncRpc) {
  auto result = ProcessCommandLineArgs(
      "gen_async_rpc=Async1"
      ",product_path=google/cloud/spanner/"
      ",emulator_endpoint_env_var=SPANNER_EMULATOR_HOST"
      ",gen_async_rpc=Async2");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result,
              Contains(Pair("gen_async_rpcs",
                            AllOf(HasSubstr("Async1"), HasSubstr("Async2")))));
}

TEST(ProcessCommandLineArgs, ProcessArgAsyncOnlyRpc) {
  auto result = ProcessCommandLineArgs(
      ",product_path=google/cloud/spanner/"
      ",emulator_endpoint_env_var=SPANNER_EMULATOR_HOST"
      ",omit_rpc=AsyncOnly"
      ",gen_async_rpc=AsyncOnly");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, Contains(Pair("omitted_rpcs", HasSubstr("AsyncOnly"))));
  EXPECT_THAT(*result,
              Contains(Pair("gen_async_rpcs", HasSubstr("AsyncOnly"))));
}

TEST(ProcessCommandLineArgs, ProcessArgNamespaceAlias) {
  auto result = ProcessCommandLineArgs(
      ",product_path=google/cloud/spanner/"
      ",emulator_endpoint_env_var=SPANNER_EMULATOR_HOST"
      ",backwards_compatibility_namespace_alias=true");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, Contains(Pair("backwards_compatibility_namespace_alias",
                                     HasSubstr("true"))));
}

TEST(ProcessCommandLineArgs, ProcessOmitClient) {
  auto result = ProcessCommandLineArgs(
      "product_path=google/cloud/spanner/"
      ",omit_client=true");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, Contains(Pair("omit_client", "true")));
}

TEST(ProcessCommandLineArgs, ProcessOmitConnection) {
  auto result = ProcessCommandLineArgs(
      "product_path=google/cloud/spanner/"
      ",omit_connection=true");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, Contains(Pair("omit_connection", "true")));
}

TEST(ProcessCommandLineArgs, ProcessOmitStubFactory) {
  auto result = ProcessCommandLineArgs(
      "product_path=google/cloud/spanner/"
      ",omit_stub_factory=true");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, Contains(Pair("omit_stub_factory", "true")));
}

TEST(ProcessCommandLineArgs, ProcessOmitStreamingUpdater) {
  auto result = ProcessCommandLineArgs(
      "product_path=google/cloud/bigquery/storage/v1/"
      ",omit_streaming_updater=true");
  ASSERT_THAT(result,
              IsOkAndHolds(Contains(Pair("omit_streaming_updater", "true"))));
}

TEST(ProcessCommandLineArgs, ProcessGenerateRoundRobinGenerator) {
  auto result = ProcessCommandLineArgs(
      "product_path=google/cloud/spanner/"
      ",generate_round_robin_decorator=true");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result,
              Contains(Pair("generate_round_robin_decorator", "true")));
}

TEST(ProcessCommandLineArgs, ProcessEndpointLocationStyle) {
  auto result = ProcessCommandLineArgs(
      "product_path=google/cloud/spanner/"
      ",endpoint_location_style=LOCATION_DEPENDENT_COMPAT");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, Contains(Pair("endpoint_location_style",
                                     "LOCATION_DEPENDENT_COMPAT")));
}

TEST(ProcessCommandLineArgs, ProcessExperimental) {
  auto result = ProcessCommandLineArgs(
      "product_path=google/cloud/spanner/"
      ",experimental=true");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, Contains(Pair("experimental", "true")));
}

TEST(ProcessCommandLineArgs, ProcessArgForwardingProductPath) {
  auto result = ProcessCommandLineArgs(
      "product_path=/google/cloud/spanner/v1"
      ",forwarding_product_path=google/cloud/spanner");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, Contains(Pair("forwarding_product_path",
                                     "google/cloud/spanner/")));
}

TEST(ProcessCommandLineArgs, ProcessServiceNameMapping) {
  auto result = ProcessCommandLineArgs(
      "product_path=google/cloud/pubsub/"
      ",service_name_mapping=old_name1=new_name1"
      ",service_name_mapping=old_name2=new_name2");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, Contains(Pair("service_name_mappings",
                                     AllOf(HasSubstr("old_name1=new_name1"),
                                           HasSubstr("old_name2=new_name2")))));
}

TEST(ProcessCommandLineArgs, ProcessServiceNameToComment) {
  auto result = ProcessCommandLineArgs(
      "product_path=google/cloud/pubsub/"
      ",service_name_to_comment=name1=comment1"
      ",service_name_to_comment=name2=comment2");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, Contains(Pair("service_name_to_comments",
                                     AllOf(HasSubstr("name1=comment1"),
                                           HasSubstr("name2=comment2")))));
}

TEST(SafeReplaceAll, Success) {
  EXPECT_EQ("one@two", SafeReplaceAll("one,two", ",", "@"));
}

TEST(SafeReplaceAll, Death) {
  EXPECT_DEATH_IF_SUPPORTED(SafeReplaceAll("one@two", ",", "@"),
                            ContainsRegex(R"(found "@" in "one@two")"));
}

TEST(CapitalizeFirstLetter, StartsWithLowerCase) {
  auto result = CapitalizeFirstLetter("foo");
  EXPECT_THAT(result, Eq("Foo"));
}

TEST(CapitalizeFirstLetter, StartsWithUpperCase) {
  auto result = CapitalizeFirstLetter("Foo");
  EXPECT_THAT(result, Eq("Foo"));
}

struct FormatCommentBlockTestParams {
  std::string comment;
  std::size_t indent_level;
  std::string introducer;
  std::size_t indent_width;
  std::size_t line_length;
  std::string result;
};

class FormatCommentBlockTest
    : public ::testing::TestWithParam<FormatCommentBlockTestParams> {};

TEST_P(FormatCommentBlockTest, CommentBlockFormattedCorrectly) {
  EXPECT_THAT(std::string("\n") + FormatCommentBlock(GetParam().comment,
                                                     GetParam().indent_level,
                                                     GetParam().introducer,
                                                     GetParam().indent_width,
                                                     GetParam().line_length),
              Eq(GetParam().result));
}

auto constexpr kSingleWordComment = "brief";
auto constexpr kLongSingleWordComment = "supercalifragilisticexpialidocious";
auto constexpr kShortComment = "This is a comment.";
auto constexpr k77CharComment =
    "The comment is not less than, not greater than, but is exactly 77 "
    "characters.";
auto constexpr kContainsMarkdownBulletedLongUrlComment =
    "Represents an IP Address resource. Google Compute Engine has two IP "
    "Address resources: * [Global (external and "
    "internal)](https://cloud.google.com/compute/docs/reference/rest/v1/"
    "globalAddresses) * [Regional (external and "
    "internal)](https://cloud.google.com/compute/docs/reference/rest/v1/"
    "addresses) For more information, see Reserving a static external IP "
    "address.";

using FormatCommentBlockDeathTest = FormatCommentBlockTest;

TEST_F(FormatCommentBlockDeathTest, LineLengthSmallerThanCommentIntro) {
  EXPECT_DEATH(FormatCommentBlock(kShortComment, 0, "", 0, 0), "");
}

INSTANTIATE_TEST_SUITE_P(
    CommentBlockFormattedCorrectly, FormatCommentBlockTest,
    Values(
        FormatCommentBlockTestParams{"", 0, "", 0, 0, R"""(
)"""},
        FormatCommentBlockTestParams{kSingleWordComment, 0, "", 0, 1, R"""(
brief)"""},
        FormatCommentBlockTestParams{kSingleWordComment, 0, "", 0, 80, R"""(
brief)"""},
        FormatCommentBlockTestParams{kLongSingleWordComment, 0, "// ", 2, 40,
                                     R"""(
// supercalifragilisticexpialidocious)"""},
        FormatCommentBlockTestParams{
            absl::StrCat(kLongSingleWordComment, " w", kLongSingleWordComment),
            0, "// ", 2, 30, R"""(
// supercalifragilisticexpialidocious
// wsupercalifragilisticexpialidocious)"""},
        FormatCommentBlockTestParams{kSingleWordComment, 0, "// ", 2, 80, R"""(
// brief)"""},
        FormatCommentBlockTestParams{kShortComment, 0, "// ", 2, 80, R"""(
// This is a comment.)"""},
        FormatCommentBlockTestParams{kShortComment, 1, "// ", 2, 80, R"""(
  // This is a comment.)"""},
        FormatCommentBlockTestParams{kShortComment, 2, "// ", 2, 80, R"""(
    // This is a comment.)"""},
        FormatCommentBlockTestParams{k77CharComment, 0, "// ", 2, 80, R"""(
// The comment is not less than, not greater than, but is exactly 77 characters.)"""},
        FormatCommentBlockTestParams{k77CharComment, 1, "// ", 2, 80, R"""(
  // The comment is not less than, not greater than, but is exactly 77
  // characters.)"""},
        FormatCommentBlockTestParams{k77CharComment, 2, "// ", 2, 80, R"""(
    // The comment is not less than, not greater than, but is exactly 77
    // characters.)"""},
        FormatCommentBlockTestParams{k77CharComment, 0, "# ", 4, 40, R"""(
# The comment is not less than, not
# greater than, but is exactly 77
# characters.)"""},
        FormatCommentBlockTestParams{k77CharComment, 1, "# ", 4, 40, R"""(
    # The comment is not less than, not
    # greater than, but is exactly 77
    # characters.)"""},
        FormatCommentBlockTestParams{k77CharComment, 2, "# ", 4, 40, R"""(
        # The comment is not less than,
        # not greater than, but is
        # exactly 77 characters.)"""},
        FormatCommentBlockTestParams{"line1 uhoh", 0, "", 0, 5, R"""(
line1
uhoh)"""},
        FormatCommentBlockTestParams{"foo wordthatiswaytoolong", 0, "", 0, 5,
                                     R"""(
foo
wordthatiswaytoolong)"""},
        // TODO(#11019): Improve formatting of comments containing markdown.
        FormatCommentBlockTestParams{kContainsMarkdownBulletedLongUrlComment, 0,
                                     "// ", 2, 80, R"""(
// Represents an IP Address resource. Google Compute Engine has two IP Address
// resources: * [Global (external and
// internal)](https://cloud.google.com/compute/docs/reference/rest/v1/globalAddresses)
// * [Regional (external and
// internal)](https://cloud.google.com/compute/docs/reference/rest/v1/addresses)
// For more information, see Reserving a static external IP address.)"""}));

struct FormatCommentKeyValueListTestParams {
  std::vector<std::pair<std::string, std::string>> comment;
  std::size_t indent_level;
  std::string separator;
  std::string introducer;
  std::size_t indent_width;
  std::size_t line_length;
  std::string result;
};

class FormatCommentKeyValueListTest
    : public ::testing::TestWithParam<FormatCommentKeyValueListTestParams> {};

TEST_P(FormatCommentKeyValueListTest, CommentKeyValueListFormattedCorrectly) {
  EXPECT_THAT(
      std::string("\n") + FormatCommentKeyValueList(
                              GetParam().comment, GetParam().indent_level,
                              GetParam().separator, GetParam().introducer,
                              GetParam().indent_width, GetParam().line_length),
      Eq(GetParam().result));
}

INSTANTIATE_TEST_SUITE_P(
    CommentKeyValueListFormattedCorrectly, FormatCommentKeyValueListTest,
    Values(
        FormatCommentKeyValueListTestParams{{}, 0, "", "", 0, 0, R"""(
)"""},
        FormatCommentKeyValueListTestParams{
            {std::make_pair("key", "value")}, 0, ":", "// ", 2, 80, R"""(
// key: value)"""},
        FormatCommentKeyValueListTestParams{
            {std::make_pair("key", k77CharComment)}, 1, ":", "// ", 2, 80, R"""(
  // key: The comment is not less than, not greater than, but is exactly 77
  // characters.)"""},
        FormatCommentKeyValueListTestParams{
            {std::make_pair(k77CharComment, k77CharComment)},
            2,
            ":",
            "// ",
            2,
            80,
            R"""(
    // The comment is not less than, not greater than, but is exactly 77
    // characters.: The comment is not less than, not greater than, but is
    // exactly 77 characters.)"""}));

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
