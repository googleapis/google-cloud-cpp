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

#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/absl_str_replace_quiet.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/status_matchers.h"
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

using ::testing::ElementsAreArray;

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

class GeneratorIntegrationTest : public testing::TestWithParam<std::string> {
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
    if (!output_path_.empty() && output_path_.back() != '/') {
      output_path_ += '/';
    }

    google::cloud::generator::Generator generator;
    google::protobuf::compiler::CommandLineInterface cli;
    cli.RegisterGenerator("--cpp_codegen_out", "--cpp_codegen_opt", &generator,
                          "Codegen C++ Generator");

    product_path_ = "generator/integration_tests/golden/";
    copyright_year_ = "2022";
    omit_rpc1_ = "Omitted1";
    omit_rpc2_ = "GoldenKitchenSink.Omitted2";
    service_endpoint_env_var_ = "GOLDEN_KITCHEN_SINK_ENDPOINT";
    emulator_endpoint_env_var_ = "GOLDEN_KITCHEN_SINK_EMULATOR_HOST";
    gen_async_rpc1_ = "GetDatabase";
    gen_async_rpc2_ = "DropDatabase";
    retry_code1_ = "GoldenKitchenSink.kInternal";
    retry_code2_ = "kUnavailable";
    retry_code3_ = "GoldenThingAdmin.kDeadlineExceeded";
    additional_proto_file_ = "generator/integration_tests/backup.proto";

    std::vector<std::string> args;
    // empty arg keeps first real arg from being ignored.
    args.emplace_back("");
    args.emplace_back("--proto_path=" + proto_path);
    args.emplace_back("--proto_path=" + googleapis_path);
    args.emplace_back("--proto_path=" + code_path);
    args.emplace_back("--cpp_codegen_out=" + output_path_);
    args.emplace_back("--cpp_codegen_opt=product_path=" + product_path_);
    args.emplace_back("--cpp_codegen_opt=copyright_year=" + copyright_year_);
    args.emplace_back("--cpp_codegen_opt=omit_rpc=" + omit_rpc1_);
    args.emplace_back("--cpp_codegen_opt=omit_rpc=" + omit_rpc2_);
    args.emplace_back("--cpp_codegen_opt=service_endpoint_env_var=" +
                      service_endpoint_env_var_);
    args.emplace_back("--cpp_codegen_opt=emulator_endpoint_env_var=" +
                      emulator_endpoint_env_var_);
    args.emplace_back("--cpp_codegen_opt=gen_async_rpc=" + gen_async_rpc1_);
    args.emplace_back("--cpp_codegen_opt=gen_async_rpc=" + gen_async_rpc2_);
    args.emplace_back("--cpp_codegen_opt=retry_status_code=" + retry_code1_);
    args.emplace_back("--cpp_codegen_opt=retry_status_code=" + retry_code2_);
    args.emplace_back("--cpp_codegen_opt=retry_status_code=" + retry_code3_);
    args.emplace_back("--cpp_codegen_opt=additional_proto_file=" +
                      additional_proto_file_);
    args.emplace_back("generator/integration_tests/test.proto");
    // It's redundant to add backup.proto as it's imported by test.proto, but
    // it lets us test the additional_proto_file flag.
    args.emplace_back("generator/integration_tests/backup.proto");

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
  std::string copyright_year_;
  std::string omit_rpc1_;
  std::string omit_rpc2_;
  std::string service_endpoint_env_var_;
  std::string emulator_endpoint_env_var_;
  std::string gen_async_rpc1_;
  std::string gen_async_rpc2_;
  std::string retry_code1_;
  std::string retry_code2_;
  std::string retry_code3_;
  std::string additional_proto_file_;
};

TEST_P(GeneratorIntegrationTest, CompareGeneratedToGolden) {
  auto golden_file = ReadFile(absl::StrCat(golden_path_, GetParam()));
  ASSERT_STATUS_OK(golden_file);
  auto generated_file =
      ReadFile(absl::StrCat(output_path_, product_path_, GetParam()));
  ASSERT_STATUS_OK(generated_file);

  EXPECT_THAT(*golden_file,
              ElementsAreArray(generated_file->begin(), generated_file->end()));
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
        "internal/golden_thing_admin_retry_traits.h",
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
        "internal/golden_kitchen_sink_retry_traits.h",
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
