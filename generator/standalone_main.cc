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

#include "generator/generator.h"
#include "generator/generator_config.pb.h"
#include "generator/internal/codegen_utils.h"
#include "generator/internal/descriptor_utils.h"
#include "generator/internal/discovery_to_proto.h"
#include "generator/internal/scaffold_generator.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/log.h"
#include "google/cloud/status_or.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/match.h"
#include <google/protobuf/compiler/command_line_interface.h>
#include <google/protobuf/text_format.h>
#include <algorithm>
#include <fstream>
#include <future>
#include <iostream>

ABSL_FLAG(std::string, log_level, "NOTICE",
          "Set to \"INFO\", \"DEBUG\", or \"TRACE\" for additional logging.");
ABSL_FLAG(std::string, config_file, "",
          "Text proto configuration file specifying the code to be generated.");
ABSL_FLAG(std::string, protobuf_proto_path, "",
          "Path to root dir of protos distributed with protobuf.");
ABSL_FLAG(std::string, googleapis_proto_path, "",
          "Path to root dir of protos distributed with googleapis.");
ABSL_FLAG(std::string, golden_proto_path, "",
          "Path to root dir of protos distributed with googleapis.");
ABSL_FLAG(std::string, discovery_proto_path, "",
          "Path to root dir of protos created from discovery documents.");
ABSL_FLAG(std::string, output_path, ".",
          "Path to root dir where code is emitted.");
ABSL_FLAG(std::string, export_output_path, ".",
          "Path to root dir where *_export.h files are emitted.");
ABSL_FLAG(std::string, scaffold_templates_path, ".",
          "Path to directory where we store scaffold templates.");
ABSL_FLAG(std::string, scaffold, "",
          "Generate the library support files for the given directory.");
ABSL_FLAG(bool, experimental_scaffold, false,
          "Generate experimental library support files.");
ABSL_FLAG(bool, update_ci, true, "Update the CI support files.");
ABSL_FLAG(bool, check_parameter_comment_substitutions, false,
          "Check that the built-in parameter comment substitutions applied.");
ABSL_FLAG(bool, generate_discovery_protos, false,
          "Generate only .proto files, no C++ code.");

namespace {

using ::google::cloud::generator_internal::GenerateMetadata;
using ::google::cloud::generator_internal::GenerateScaffold;
using ::google::cloud::generator_internal::LibraryName;
using ::google::cloud::generator_internal::LibraryPath;
using ::google::cloud::generator_internal::LoadApiIndex;
using ::google::cloud::generator_internal::SafeReplaceAll;
using ::google::cloud::generator_internal::ScaffoldVars;

struct CommandLineArgs {
  std::string config_file;
  std::string protobuf_proto_path;
  std::string googleapis_proto_path;
  std::string golden_proto_path;
  std::string discovery_proto_path;
  std::string output_path;
  std::string export_output_path;
  std::string scaffold_templates_path;
  std::string scaffold;
  bool experimental_scaffold;
  bool update_ci;
  bool generate_discovery_protos;
};

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
  auto services = config.service();
  for (auto const& p : config.discovery_products()) {
    services.Add(p.rest_services().begin(), p.rest_services().end());
  }

  for (auto const& service : services) {
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
    // Services without a connection do not create mocks.
    if (!service.omit_connection()) {
      install_directories.push_back("./include/" + product_path + "/mocks");
    }
    auto const& forwarding_product_path = service.forwarding_product_path();
    if (!forwarding_product_path.empty()) {
      install_directories.push_back("./include/" + forwarding_product_path);
      if (!service.omit_connection()) {
        install_directories.push_back("./include/" + forwarding_product_path +
                                      "/mocks");
      }
    }
    auto const lib = LibraryName(product_path);
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

google::cloud::Status GenerateProtosForRestProducts(
    CommandLineArgs const& generator_args,
    google::protobuf::RepeatedPtrField<
        google::cloud::cpp::generator::DiscoveryDocumentDefinedProduct> const&
        rest_products) {
  google::protobuf::RepeatedPtrField<
      google::cloud::cpp::generator::ServiceConfiguration>
      services;

  for (auto const& p : rest_products) {
    auto doc = google::cloud::generator_internal::GetDiscoveryDoc(
        p.discovery_document_url());
    if (!doc) return std::move(doc).status();
    auto status =
        google::cloud::generator_internal::GenerateProtosFromDiscoveryDoc(
            *doc, p.discovery_document_url(),
            generator_args.protobuf_proto_path,
            generator_args.googleapis_proto_path, generator_args.output_path,
            generator_args.export_output_path,
            std::set<std::string>(p.operation_services().begin(),
                                  p.operation_services().end()));
    if (!status.ok()) return status;
  }

  return {};
}

std::vector<std::future<google::cloud::Status>> GenerateCodeFromProtos(
    CommandLineArgs const& generator_args,
    google::protobuf::RepeatedPtrField<
        google::cloud::cpp::generator::ServiceConfiguration> const& services) {
  std::vector<std::future<google::cloud::Status>> tasks;
  auto const api_index = LoadApiIndex(generator_args.googleapis_proto_path);
  for (auto const& service : services) {
    auto scaffold_vars =
        ScaffoldVars(generator_args.googleapis_proto_path, api_index, service,
                     generator_args.experimental_scaffold);
    auto const generate_scaffold =
        LibraryPath(service.product_path()) == generator_args.scaffold;
    if (generate_scaffold) {
      GenerateScaffold(scaffold_vars, generator_args.scaffold_templates_path,
                       generator_args.output_path, service);
    }
    if (!service.omit_repo_metadata()) {
      GenerateMetadata(scaffold_vars, generator_args.output_path, service,
                       generate_scaffold);
    }

    std::vector<std::string> args;
    // empty arg prevents first real arg from being ignored.
    args.emplace_back("");
    args.emplace_back("--proto_path=" + generator_args.protobuf_proto_path);
    args.emplace_back("--proto_path=" + generator_args.googleapis_proto_path);
    if (!generator_args.golden_proto_path.empty()) {
      args.emplace_back("--proto_path=" + generator_args.golden_proto_path);
    }
    if (!generator_args.discovery_proto_path.empty()) {
      args.emplace_back("--proto_path=" + generator_args.discovery_proto_path);
    }
    args.emplace_back("--cpp_codegen_out=" + generator_args.output_path);
    args.emplace_back("--cpp_codegen_opt=product_path=" +
                      service.product_path());
    args.emplace_back("--cpp_codegen_opt=copyright_year=" +
                      service.initial_copyright_year());
    for (auto const& omit_service : service.omitted_services()) {
      args.emplace_back("--cpp_codegen_opt=omit_service=" + omit_service);
    }
    for (auto const& omit_rpc : service.omitted_rpcs()) {
      args.emplace_back(absl::StrCat("--cpp_codegen_opt=omit_rpc=",
                                     SafeReplaceAll(omit_rpc, ",", "@")));
    }
    for (auto const& emit_rpc : service.emitted_rpcs()) {
      args.emplace_back(absl::StrCat("--cpp_codegen_opt=emit_rpc=",
                                     SafeReplaceAll(emit_rpc, ",", "@")));
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
    if (service.experimental()) {
      args.emplace_back("--cpp_codegen_opt=experimental=true");
    }
    if (!service.forwarding_product_path().empty()) {
      args.emplace_back("--cpp_codegen_opt=forwarding_product_path=" +
                        service.forwarding_product_path());
    }
    for (auto const& o : service.idempotency_overrides()) {
      args.emplace_back(absl::StrCat(
          "--cpp_codegen_opt=idempotency_override=", o.rpc_name(), ":",
          google::cloud::cpp::generator::ServiceConfiguration::
              IdempotencyOverride::Idempotency_Name(o.idempotency())));
    }

    // Unless generate_grpc_transport has been explicitly set to false, treat it
    // as having a default value of true.
    if (service.has_generate_grpc_transport() &&
        !service.generate_grpc_transport()) {
      args.emplace_back("--cpp_codegen_opt=generate_grpc_transport=false");
    } else {
      args.emplace_back("--cpp_codegen_opt=generate_grpc_transport=true");
    }

    if (service.proto_file_source() ==
        google::cloud::cpp::generator::ServiceConfiguration::
            DISCOVERY_DOCUMENT) {
      args.emplace_back(
          "--cpp_codegen_opt=proto_file_source=discovery_document");
    } else {
      args.emplace_back("--cpp_codegen_opt=proto_file_source=googleapis");
    }

    // Unless preserve_proto_field_names_in_json has been explicitly set to
    // true, treat it as having a default value of false.
    if (service.preserve_proto_field_names_in_json()) {
      args.emplace_back(
          "--cpp_codegen_opt=preserve_proto_field_names_in_json=true");
    } else {
      args.emplace_back(
          "--cpp_codegen_opt=preserve_proto_field_names_in_json=false");
    }

    // Add the key value pairs as a single parameter with a colon delimiter.
    for (auto const& [key, value] : service.service_name_mapping()) {
      args.emplace_back(absl::StrCat(
          "--cpp_codegen_opt=service_name_mapping=", key, ":", value));
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
  return tasks;
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
  int rc = 0;
  absl::ParseCommandLine(argc, argv);
  auto log_level = google::cloud::ParseSeverity(absl::GetFlag(FLAGS_log_level));
  if (!log_level || *log_level > google::cloud::Severity::GCP_LS_NOTICE) {
    log_level = google::cloud::Severity::GCP_LS_NOTICE;
  }
  // A default backend is already in place, so we must remove it first.
  google::cloud::LogSink::DisableStdClog();
  google::cloud::LogSink::EnableStdClog(*log_level);
  if (*log_level <
      google::cloud::Severity::GOOGLE_CLOUD_CPP_LOGGING_MIN_SEVERITY_ENABLED) {
    GCP_LOG(WARNING)
        << "Log level " << *log_level
        << " is less than the minimum enabled level of "
        << google::cloud::Severity::
               GOOGLE_CLOUD_CPP_LOGGING_MIN_SEVERITY_ENABLED
        << "; you'll need to recompile everything for that to work";
  }

  CommandLineArgs const args{absl::GetFlag(FLAGS_config_file),
                             absl::GetFlag(FLAGS_protobuf_proto_path),
                             absl::GetFlag(FLAGS_googleapis_proto_path),
                             absl::GetFlag(FLAGS_golden_proto_path),
                             absl::GetFlag(FLAGS_discovery_proto_path),
                             absl::GetFlag(FLAGS_output_path),
                             absl::GetFlag(FLAGS_export_output_path),
                             absl::GetFlag(FLAGS_scaffold_templates_path),
                             absl::GetFlag(FLAGS_scaffold),
                             absl::GetFlag(FLAGS_experimental_scaffold),
                             absl::GetFlag(FLAGS_update_ci),
                             absl::GetFlag(FLAGS_generate_discovery_protos)};

  GCP_LOG(INFO) << "proto_path = " << args.protobuf_proto_path << "\n";
  GCP_LOG(INFO) << "googleapis_path = " << args.googleapis_proto_path << "\n";
  GCP_LOG(INFO) << "config_file = " << args.config_file << "\n";
  GCP_LOG(INFO) << "output_path = " << args.output_path << "\n";
  GCP_LOG(INFO) << "export_output_path = " << args.export_output_path << "\n";

  auto config = GetConfig(args.config_file);
  if (!config.ok()) {
    GCP_LOG(FATAL) << "Failed to parse config file: " << args.config_file
                   << "\n";
  }

  if (args.update_ci) {
    auto const install_result =
        WriteInstallDirectories(*config, args.output_path);
    if (install_result != 0) return install_result;
  }

  if (args.generate_discovery_protos) {
    auto result =
        GenerateProtosForRestProducts(args, config->discovery_products());
    if (!result.ok()) GCP_LOG(FATAL) << result;
    return 0;
  }

  // Start generating C++ code for services defined in googleapis protos.
  auto tasks = GenerateCodeFromProtos(args, config->service());

  google::protobuf::RepeatedPtrField<
      google::cloud::cpp::generator::ServiceConfiguration>
      rest_services;
  for (auto const& p : config->discovery_products()) {
    rest_services.Add(p.rest_services().begin(), p.rest_services().end());
  }
  for (auto& r : rest_services) {
    r.set_proto_file_source(google::cloud::cpp::generator::
                                ServiceConfiguration::DISCOVERY_DOCUMENT);
  }

  // Generate C++ code from those generated protos.
  auto rest_tasks = GenerateCodeFromProtos(args, rest_services);

  std::string error_message;
  tasks.insert(tasks.end(), std::make_move_iterator(rest_tasks.begin()),
               std::make_move_iterator(rest_tasks.end()));
  for (auto& t : tasks) {
    auto result = t.get();
    if (!result.ok()) {
      GCP_LOG(ERROR) << result;
      rc = 1;
    }
  }

  // If we were asked to check the parameter comment substitutions, and some
  // went unused, emit a fatal error so that we might remove/fix them. The
  // substitutions should probably be part of the config file (rather than
  // being built in) so that the check could be unconditional (instead of
  // flag-based).
  if (absl::GetFlag(FLAGS_check_parameter_comment_substitutions) &&
      !google::cloud::generator_internal::
          CheckParameterCommentSubstitutions()) {
    GCP_LOG(FATAL) << "Remove unused parameter comment substitution(s)";
  }

  return rc;
}
