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
            .value_or("/v");

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
};

TEST_P(GeneratorIntegrationTest, CompareGeneratedToGolden) {
  auto golden_file = ReadFile(absl::StrCat(golden_path_, GetParam()));
  EXPECT_TRUE(golden_file.ok());
  auto generated_file =
      ReadFile(absl::StrCat(output_path_, product_path_, GetParam()));

  EXPECT_TRUE(generated_file.ok());
  EXPECT_EQ(golden_file->size(), generated_file->size());
  for (unsigned int i = 0; i < golden_file->size(); ++i) {
    EXPECT_EQ((*golden_file)[i], (*generated_file)[i]);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Generator, GeneratorIntegrationTest,
    testing::Values(
        "database_admin_client.gcpcxx.pb.h",
        "database_admin_client.gcpcxx.pb.cc",
        "database_admin_connection.gcpcxx.pb.h",
        "database_admin_connection.gcpcxx.pb.cc",
        "database_admin_connection_idempotency_policy.gcpcxx.pb.h",
        "database_admin_connection_idempotency_policy.gcpcxx.pb.cc",
        "internal/database_admin_logging_decorator.gcpcxx.pb.h",
        "internal/database_admin_logging_decorator.gcpcxx.pb.cc",
        "internal/database_admin_metadata_decorator.gcpcxx.pb.h",
        "internal/database_admin_metadata_decorator.gcpcxx.pb.cc",
        "internal/database_admin_stub_factory.gcpcxx.pb.h",
        "internal/database_admin_stub_factory.gcpcxx.pb.cc",
        "internal/database_admin_stub.gcpcxx.pb.h",
        "internal/database_admin_stub.gcpcxx.pb.cc",
        "mocks/mock_database_admin_connection.gcpcxx.pb.h",
        "iam_credentials_client.gcpcxx.pb.h",
        "iam_credentials_client.gcpcxx.pb.cc",
        "iam_credentials_connection.gcpcxx.pb.h",
        "iam_credentials_connection.gcpcxx.pb.cc",
        "iam_credentials_connection_idempotency_policy.gcpcxx.pb.h",
        "iam_credentials_connection_idempotency_policy.gcpcxx.pb.cc",
        "internal/iam_credentials_logging_decorator.gcpcxx.pb.h",
        "internal/iam_credentials_logging_decorator.gcpcxx.pb.cc",
        "internal/iam_credentials_metadata_decorator.gcpcxx.pb.h",
        "internal/iam_credentials_metadata_decorator.gcpcxx.pb.cc",
        "internal/iam_credentials_stub_factory.gcpcxx.pb.h",
        "internal/iam_credentials_stub_factory.gcpcxx.pb.cc",
        "internal/iam_credentials_stub.gcpcxx.pb.h",
        "internal/iam_credentials_stub.gcpcxx.pb.cc",
        "mocks/mock_iam_credentials_connection.gcpcxx.pb.h"),
    [](testing::TestParamInfo<GeneratorIntegrationTest::ParamType> const&
           info) {
      return absl::StrReplaceAll(std::string(info.param),
                                 {{".", "_"}, {"/", "_"}});
    });

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
