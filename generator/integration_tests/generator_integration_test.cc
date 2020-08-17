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

#include "google/cloud/internal/getenv.h"
#include "google/cloud/status_or.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "generator/generator.h"
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

class GeneratorIntegrationTest : public testing::Test {
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

    // Path to location where generated code is written.
    auto output_path = google::cloud::internal::GetEnv(
                           "GOOGLE_CLOUD_CPP_GENERATOR_OUTPUT_PATH")
                           .value_or(::testing::TempDir());

    google::cloud::generator::Generator generator;
    google::protobuf::compiler::CommandLineInterface cli;
    cli.RegisterGenerator("--cpp_codegen_out", "--cpp_codegen_opt", &generator,
                          "Codegen C++ Generator");

    std::vector<std::string> args;
    // empty arg keeps first real arg from being ignored.
    args.emplace_back("");
    args.emplace_back("--proto_path=" + proto_path);
    args.emplace_back("--proto_path=" + googleapis_path);
    args.emplace_back("--proto_path=" + code_path);
    args.emplace_back("--cpp_codegen_out=" + output_path);
    args.emplace_back("--cpp_codegen_opt=product_path=google/cloud/test/");
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
};

TEST_F(GeneratorIntegrationTest, StubHeader) {
  auto golden_path =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_GENERATOR_GOLDEN_PATH")
          .value_or("");
  auto golden_file = ReadFile(golden_path +
                              "generator/integration_tests/golden/"
                              "database_admin_stub.gcpcxx.pb.h.golden");
  EXPECT_TRUE(golden_file.ok());

  auto output_path =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_GENERATOR_OUTPUT_PATH")
          .value_or(::testing::TempDir());

  auto generated_file =
      ReadFile(output_path +
               "/google/cloud/test/internal/database_admin_stub.gcpcxx.pb.h");
  EXPECT_TRUE(generated_file.ok());
  EXPECT_EQ(golden_file, generated_file);
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
