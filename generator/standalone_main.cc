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

#include "google/cloud/status_or.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "generator/generator.h"
#include "generator/generator_config.pb.h"
#include <google/protobuf/compiler/command_line_interface.h>
#include <google/protobuf/text_format.h>
#include <fstream>
#include <iostream>
#include <sstream>

ABSL_FLAG(std::string, config_file, "./generator/generator_config.textproto",
          "Text proto configuration file specifying the code to be generated.");
ABSL_FLAG(std::string, googleapis_commit_hash, "",
          "Git commit hash of googleapis dependency.");
ABSL_FLAG(std::string, protobuf_proto_path, "",
          "Path to root dir of protos distributed with protobuf.");
ABSL_FLAG(std::string, googleapis_proto_path, "",
          "Path to root dir of protos distributed with googleapis.");
ABSL_FLAG(std::string, output_path, ".", "Path to root dir where code is emitted.");
namespace {

google::cloud::StatusOr<google::cloud::cpp::generator::GeneratorConfiguration>
GetConfig(std::string const& filepath) {
  std::ifstream input(filepath);
  std::stringstream buffer;
  buffer << input.rdbuf();

  google::cloud::cpp::generator::GeneratorConfiguration generator_config;
  auto parse_result = google::protobuf::TextFormat::ParseFromString(
      buffer.str(), &generator_config);

  if (parse_result) return generator_config;
  return google::cloud::Status(google::cloud::StatusCode::kInvalidArgument,
                               "Unable to parse textproto file.");
}
}  // namespace

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);

  auto proto_path = absl::GetFlag(FLAGS_protobuf_proto_path);
  auto googleapis_path = absl::GetFlag(FLAGS_googleapis_proto_path);
  auto googleapis_commit_hash = absl::GetFlag(FLAGS_googleapis_commit_hash);
  auto config_file = absl::GetFlag(FLAGS_config_file);
  auto output_path = absl::GetFlag(FLAGS_output_path);

  auto config = GetConfig(absl::GetFlag(FLAGS_config_file));
  if (!config.ok()) {
    std::cerr << "Failed to parse config file: "
              << absl::GetFlag(FLAGS_config_file) << "\n";
  }

  int result = 0;
  for (int i = 0; result == 0 && i < config->service().size(); ++i) {
    google::protobuf::compiler::CommandLineInterface cli;
    google::cloud::generator::Generator generator;
    cli.RegisterGenerator("--cpp_codegen_out", "--cpp_codegen_opt", &generator,
                          "Codegen C++ Generator");
    google::cloud::cpp::generator::ServiceConfiguration service =
        config->service(i);
    std::vector<std::string> args;
    // empty arg prevents first real arg from being ignored.
    args.emplace_back("");
    args.emplace_back("--proto_path=" + proto_path);
    args.emplace_back("--proto_path=" + googleapis_path);
    args.emplace_back("--cpp_codegen_out=" + output_path);
    args.emplace_back("--cpp_codegen_opt=product_path=" +
                      service.product_path());
    args.emplace_back("--cpp_codegen_opt=googleapis_commit_hash=" +
                      absl::GetFlag(FLAGS_googleapis_commit_hash));
    args.emplace_back("--cpp_codegen_opt=copyright_year=" +
                      service.initial_copyright_year());
    for (auto const& omit_rpc : service.omitted_rpcs()) {
      args.emplace_back("--cpp_codegen_opt=omit_rpc=" + omit_rpc);
    }
    args.emplace_back(service.service_proto_path());

    std::vector<char const*> c_args;
    c_args.reserve(args.size());
    for (auto const& arg : args) {
      c_args.push_back(arg.c_str());
    }

    result = cli.Run(static_cast<int>(c_args.size()), c_args.data());
  }
  return result;
}
