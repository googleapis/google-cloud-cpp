// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/absl_str_replace_quiet.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "generator/generator.h"
#include "generator/internal/codegen_utils.h"
#include <google/protobuf/compiler/command_line_interface.h>
#include <gmock/gmock.h>
#include <fstream>
#include <iostream>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::google::cloud::testing_util::IsOk;

StatusOr<std::vector<std::string>> ReadFile(std::string const& filepath) {
  std::string line;
  std::vector<std::string> file_contents;
  std::ifstream input_file(filepath);

  if (!input_file)
    return Status(StatusCode::kNotFound, "Cannot open: " + filepath);
  while (std::getline(input_file, line)) {
    file_contents.push_back(line);
  }
  return file_contents;
}

class GeneratorIntegrationTest
    : public testing::TestWithParam<absl::string_view> {
 protected:
  void SetUp() override {
    auto run_integration_tests =
        google::cloud::internal::GetEnv(
            "GOOGLE_CLOUD_CPP_GENERATOR_RUN_INTEGRATION_TESTS")
            .value_or("");
    if (run_integration_tests != "yes") {
      GTEST_SKIP();
    }

    EXPECT_EQ(run_integration_tests, "yes");
    ASSERT_TRUE(
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_GENERATOR_PROTO_PATH")
            .has_value());
    ASSERT_TRUE(google::cloud::internal::GetEnv(
                    "GOOGLE_CLOUD_CPP_GENERATOR_GOOGLEAPIS_PATH")
                    .has_value());

    // Path to find .proto files distributed with protobuf.
    auto proto_path =
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_GENERATOR_PROTO_PATH")
            .value();

    // Path to find .proto files distributed with googleapis/googleapis repo.
    auto googleapis_path = google::cloud::internal::GetEnv(
                               "GOOGLE_CLOUD_CPP_GENERATOR_GOOGLEAPIS_PATH")
                               .value();

    // Path to find .proto files defined for these tests.
    auto code_path =
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_GENERATOR_CODE_PATH")
            .value_or("/workspace");

    golden_path_ = google::cloud::internal::GetEnv(
                       "GOOGLE_CLOUD_CPP_GENERATOR_GOLDEN_PATH")
                       .value_or("") +
                   "generator/integration_tests/golden/";

    // Path to location where generated code is written.
    output_path_ = google::cloud::internal::GetEnv(
                       "GOOGLE_CLOUD_CPP_GENERATOR_OUTPUT_PATH")
                       .value_or(::testing::TempDir());

    google::cloud::generator::Generator generator;
    google::protobuf::compiler::CommandLineInterface cli;
    cli.RegisterGenerator("--cpp_codegen_out", "--cpp_codegen_opt", &generator,
                          "Codegen C++ Generator");

    product_path_ = "generator/integration_tests/golden/";
    googleapis_commit_hash_ = "59f97e6044a1275f83427ab7962a154c00d915b5";
    copyright_year_ = CurrentCopyrightYear();
    omit_rpc1_ = "Omitted1";
    omit_rpc2_ = "Omitted2";

    std::vector<std::string> args;
    // empty arg keeps first real arg from being ignored.
    args.emplace_back("");
    args.emplace_back("--proto_path=" + proto_path);
    args.emplace_back("--proto_path=" + googleapis_path);
    args.emplace_back("--proto_path=" + code_path);
    args.emplace_back("--cpp_codegen_out=" + output_path_);
    args.emplace_back("--cpp_codegen_opt=product_path=" + product_path_);
    args.emplace_back("--cpp_codegen_opt=googleapis_commit_hash=" +
                      googleapis_commit_hash_);
    args.emplace_back("--cpp_codegen_opt=copyright_year=" + copyright_year_);
    args.emplace_back("--cpp_codegen_opt=omit_rpc=" + omit_rpc1_);
    args.emplace_back("--cpp_codegen_opt=omit_rpc=" + omit_rpc2_);
    args.emplace_back("generator/integration_tests/test.proto");

    std::vector<char const*> c_args;
    c_args.reserve(args.size());
    for (auto const& arg : args) {
      std::cout << "args : " << arg << "\n";
      c_args.push_back(arg.c_str());
    }

    static int const kResult =
        cli.Run(static_cast<int>(c_args.size()), c_args.data());

    EXPECT_EQ(0, kResult);
  }

  std::string product_path_;
  std::string output_path_;
  std::string golden_path_;
  std::string googleapis_commit_hash_;
  std::string copyright_year_;
  std::string omit_rpc1_;
  std::string omit_rpc2_;
};

TEST_P(GeneratorIntegrationTest, CompareGeneratedToGolden) {
  auto golden_file = ReadFile(absl::StrCat(golden_path_, GetParam()));
  EXPECT_THAT(golden_file, IsOk());
  auto generated_file =
      ReadFile(absl::StrCat(output_path_, product_path_, GetParam()));

  EXPECT_THAT(generated_file, IsOk());
  EXPECT_EQ(golden_file->size(), generated_file->size());
  for (unsigned int i = 0; i < golden_file->size(); ++i) {
    EXPECT_EQ((*golden_file)[i], (*generated_file)[i]);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Generator, GeneratorIntegrationTest,
    testing::Values(
        "golden_thing_admin_client.h", "golden_thing_admin_client.cc",
        "golden_thing_admin_connection.h", "golden_thing_admin_connection.cc",
        "golden_thing_admin_connection_idempotency_policy.h",
        "golden_thing_admin_connection_idempotency_policy.cc",
        "golden_thing_admin_options.h",
        "internal/golden_thing_admin_logging_decorator.h",
        "internal/golden_thing_admin_logging_decorator.cc",
        "internal/golden_thing_admin_metadata_decorator.h",
        "internal/golden_thing_admin_metadata_decorator.cc",
        "internal/golden_thing_admin_option_defaults.h",
        "internal/golden_thing_admin_option_defaults.cc",
        "internal/golden_thing_admin_stub_factory.h",
        "internal/golden_thing_admin_stub_factory.cc",
        "internal/golden_thing_admin_stub.h",
        "internal/golden_thing_admin_stub.cc",
        "mocks/mock_golden_thing_admin_connection.h",
        "golden_kitchen_sink_client.h", "golden_kitchen_sink_client.cc",
        "golden_kitchen_sink_connection.h", "golden_kitchen_sink_connection.cc",
        "golden_kitchen_sink_connection_idempotency_policy.h",
        "golden_kitchen_sink_connection_idempotency_policy.cc",
        "golden_kitchen_sink_options.h",
        "internal/golden_kitchen_sink_logging_decorator.h",
        "internal/golden_kitchen_sink_logging_decorator.cc",
        "internal/golden_kitchen_sink_metadata_decorator.h",
        "internal/golden_kitchen_sink_metadata_decorator.cc",
        "internal/golden_kitchen_sink_option_defaults.h",
        "internal/golden_kitchen_sink_option_defaults.cc",
        "internal/golden_kitchen_sink_stub_factory.h",
        "internal/golden_kitchen_sink_stub_factory.cc",
        "internal/golden_kitchen_sink_stub.h",
        "internal/golden_kitchen_sink_stub.cc",
        "mocks/mock_golden_kitchen_sink_connection.h"),
    [](testing::TestParamInfo<GeneratorIntegrationTest::ParamType> const&
           info) {
      return absl::StrReplaceAll(std::string(info.param),
                                 {{".", "_"}, {"/", "_"}});
    });

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
