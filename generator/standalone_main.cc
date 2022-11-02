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

#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/log.h"
#include "google/cloud/status_or.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/match.h"
#include "generator/generator.h"
#include "generator/generator_config.pb.h"
#include "generator/internal/scaffold_generator.h"
#include <google/protobuf/compiler/command_line_interface.h>
#include <google/protobuf/text_format.h>
#include <algorithm>
#include <fstream>
#include <future>
#include <iostream>

ABSL_FLAG(std::string, config_file, "",
          "Text proto configuration file specifying the code to be generated.");
ABSL_FLAG(std::string, protobuf_proto_path, "",
          "Path to root dir of protos distributed with protobuf.");
ABSL_FLAG(std::string, googleapis_proto_path, "",
          "Path to root dir of protos distributed with googleapis.");
ABSL_FLAG(std::string, golden_proto_path, "",
          "Path to root dir of protos distributed with googleapis.");
ABSL_FLAG(std::string, output_path, ".",
          "Path to root dir where code is emitted.");
ABSL_FLAG(std::string, scaffold, "",
          "Generate the library support files for the given directory.");
ABSL_FLAG(bool, experimental_scaffold, false,
          "Generate experimental library support files.");
ABSL_FLAG(bool, update_ci, true, "Update the CI support files.");

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

/**
 * Return the parent directory of @path, if any.
 *
 * @note The return value for absolute paths or paths without `/` is
 *     unspecified, as we do not expect any such inputs.
 *
 * @return For a  path of the form `a/b/c` returns `a/b`.
 */
std::string Dirname(std::string const& path) {
  return path.substr(0, path.find_last_of('/'));
}

std::vector<std::string> Parents(std::string path) {
  std::vector<std::string> p;
  p.push_back(path);
  while (absl::StrContains(path, '/')) {
    path = Dirname(path);
    p.push_back(path);
  }
  return p;
}

int WriteInstallDirectories(
    google::cloud::cpp::generator::GeneratorConfiguration const& config,
    std::string const& output_path) {
  std::vector<std::string> install_directories{".", "./lib64", "./lib64/cmake"};
  for (auto const& service : config.service()) {
    if (service.product_path().empty()) {
      GCP_LOG(ERROR) << "Empty product path in config, service="
                     << service.DebugString() << "\n";
      return 1;
    }
    if (service.service_proto_path().empty()) {
      GCP_LOG(ERROR) << "Empty service proto path in config, service="
                     << service.DebugString() << "\n";
      return 1;
    }

    auto const& product_path = service.product_path();
    for (auto const& p : Parents("./include/" + product_path)) {
      install_directories.push_back(p);
    }
    install_directories.push_back("./include/" + product_path + "/internal");
    for (auto const& p :
         Parents("./include/" + Dirname(service.service_proto_path()))) {
      install_directories.push_back(p);
    }
    auto const lib = google::cloud::generator_internal::LibraryName(service);
    // Bigtable and Spanner use a custom path for generated code.
    if (lib == "admin") continue;
    // Services without a connection do not create mocks.
    if (!service.omit_connection()) {
      install_directories.push_back("./include/" + product_path + "/mocks");
    }
    install_directories.push_back("./lib64/cmake/google_cloud_cpp_" + lib);
  }
  std::sort(install_directories.begin(), install_directories.end());
  auto end =
      std::unique(install_directories.begin(), install_directories.end());
  std::ofstream of(output_path + "/ci/etc/expected_install_directories");
  std::copy(install_directories.begin(), end,
            std::ostream_iterator<std::string>(of, "\n"));
  return 0;
}

int WriteFeatureList(
    google::cloud::cpp::generator::GeneratorConfiguration const& config,
    std::string const& output_path) {
  std::vector<std::string> features;
  for (auto const& service : config.service()) {
    if (service.product_path().empty()) {
      GCP_LOG(ERROR) << "Empty product path in config, service="
                     << service.DebugString() << "\n";
      return 1;
    }
    auto feature = google::cloud::generator_internal::LibraryName(service);
    // Spanner and Bigtable use a custom directory for generated files
    if (feature == "admin") continue;
    features.push_back(std::move(feature));
  }
  std::sort(features.begin(), features.end());
  auto const end = std::unique(features.begin(), features.end());
  std::ofstream of(output_path + "/ci/etc/full_feature_list");
  std::copy(features.begin(), end,
            std::ostream_iterator<std::string>(of, "\n"));
  return 0;
}

}  // namespace

/**
 * C++ microgenerator.
 *
 * @par Command line arguments:
 *  --config-file=<path> REQUIRED should be a textproto file for
 *      GeneratorConfiguration message.
 *  --protobuf_proto_path=<path> REQUIRED path to .proto files distributed with
 *      protobuf.
 *  --googleapis_proto_path=<path> REQUIRED path to .proto files distributed
 *      with googleapis repo.
 *  --output_path=<path> OPTIONAL defaults to current directory.
 */
int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  google::cloud::LogSink::EnableStdClog(
      google::cloud::Severity::GCP_LS_WARNING);

  auto proto_path = absl::GetFlag(FLAGS_protobuf_proto_path);
  auto googleapis_path = absl::GetFlag(FLAGS_googleapis_proto_path);
  auto golden_path = absl::GetFlag(FLAGS_golden_proto_path);
  auto config_file = absl::GetFlag(FLAGS_config_file);
  auto output_path = absl::GetFlag(FLAGS_output_path);
  auto scaffold = absl::GetFlag(FLAGS_scaffold);
  auto experimental_scaffold = absl::GetFlag(FLAGS_experimental_scaffold);

  GCP_LOG(INFO) << "proto_path = " << proto_path << "\n";
  GCP_LOG(INFO) << "googleapis_path = " << googleapis_path << "\n";
  GCP_LOG(INFO) << "config_file = " << config_file << "\n";
  GCP_LOG(INFO) << "output_path = " << output_path << "\n";

  auto config = GetConfig(config_file);
  if (!config.ok()) {
    GCP_LOG(ERROR) << "Failed to parse config file: " << config_file << "\n";
  }

  if (absl::GetFlag(FLAGS_update_ci)) {
    auto const install_result = WriteInstallDirectories(*config, output_path);
    if (install_result != 0) return install_result;
    auto const features_result = WriteFeatureList(*config, output_path);
    if (features_result != 0) return features_result;
  }

  std::vector<std::future<google::cloud::Status>> tasks;
  for (auto const& service : config->service()) {
    if (service.product_path() == scaffold) {
      google::cloud::generator_internal::GenerateScaffold(
          googleapis_path, output_path, service, experimental_scaffold);
    }
    std::vector<std::string> args;
    // empty arg prevents first real arg from being ignored.
    args.emplace_back("");
    args.emplace_back("--proto_path=" + proto_path);
    args.emplace_back("--proto_path=" + googleapis_path);
    if (!golden_path.empty()) {
      args.emplace_back("--proto_path=" + golden_path);
    }
    args.emplace_back("--cpp_codegen_out=" + output_path);
    args.emplace_back("--cpp_codegen_opt=product_path=" +
                      service.product_path());
    args.emplace_back("--cpp_codegen_opt=copyright_year=" +
                      service.initial_copyright_year());
    for (auto const& omit_service : service.omitted_services()) {
      args.emplace_back("--cpp_codegen_opt=omit_service=" + omit_service);
    }
    for (auto const& omit_rpc : service.omitted_rpcs()) {
      args.emplace_back("--cpp_codegen_opt=omit_rpc=" + omit_rpc);
    }
    if (service.backwards_compatibility_namespace_alias()) {
      args.emplace_back(
          "--cpp_codegen_opt=backwards_compatibility_namespace_alias=true");
    }
    for (auto const& retry_code : service.retryable_status_codes()) {
      args.emplace_back("--cpp_codegen_opt=retry_status_code=" + retry_code);
    }
    if (service.omit_client()) {
      args.emplace_back("--cpp_codegen_opt=omit_client=true");
    }
    if (service.omit_connection()) {
      args.emplace_back("--cpp_codegen_opt=omit_connection=true");
    }
    if (service.omit_stub_factory()) {
      args.emplace_back("--cpp_codegen_opt=omit_stub_factory=true");
    }
    if (service.generate_round_robin_decorator()) {
      args.emplace_back(
          "--cpp_codegen_opt=generate_round_robin_decorator=true");
    }
    args.emplace_back("--cpp_codegen_opt=service_endpoint_env_var=" +
                      service.service_endpoint_env_var());
    args.emplace_back("--cpp_codegen_opt=emulator_endpoint_env_var=" +
                      service.emulator_endpoint_env_var());
    args.emplace_back(
        "--cpp_codegen_opt=endpoint_location_style=" +
        google::cloud::cpp::generator::ServiceConfiguration::
            EndpointLocationStyle_Name(service.endpoint_location_style()));
    for (auto const& gen_async_rpc : service.gen_async_rpcs()) {
      args.emplace_back("--cpp_codegen_opt=gen_async_rpc=" + gen_async_rpc);
    }
    for (auto const& additional_proto_file : service.additional_proto_files()) {
      args.emplace_back("--cpp_codegen_opt=additional_proto_file=" +
                        additional_proto_file);
    }
    if (service.generate_rest_transport()) {
      args.emplace_back("--cpp_codegen_opt=generate_rest_transport=true");
    }
    args.emplace_back(service.service_proto_path());
    for (auto const& additional_proto_file : service.additional_proto_files()) {
      args.emplace_back(additional_proto_file);
    }

    GCP_LOG(INFO) << "Generating service code using: "
                  << absl::StrJoin(args, ";") << "\n";

    tasks.push_back(std::async(std::launch::async, [args] {
      google::protobuf::compiler::CommandLineInterface cli;
      google::cloud::generator::Generator generator;
      cli.RegisterGenerator("--cpp_codegen_out", "--cpp_codegen_opt",
                            &generator, "Codegen C++ Generator");
      std::vector<char const*> c_args;
      c_args.reserve(args.size());
      for (auto const& arg : args) {
        c_args.push_back(arg.c_str());
      }

      if (cli.Run(static_cast<int>(c_args.size()), c_args.data()) != 0)
        return google::cloud::Status(google::cloud::StatusCode::kInternal,
                                     absl::StrCat("Generating service from ",
                                                  c_args.back(), " failed."));

      return google::cloud::Status{};
    }));
  }

  std::string error_message;
  for (auto& t : tasks) {
    auto result = t.get();
    if (!result.ok()) {
      absl::StrAppend(&error_message, result.message(), "\n");
    }
  }

  if (!error_message.empty()) {
    GCP_LOG(ERROR) << error_message;
    return 1;
  }
  return 0;
}
