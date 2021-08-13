// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/examples/bigtable_examples_common.h"
#include "google/cloud/bigtable/table_admin.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/crash_handler.h"
#include <fstream>
#include <sstream>

namespace {

using ::google::cloud::bigtable::examples::Usage;

void AccessToken(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw Usage{"test-access-token: <project-id> <instance-id> <access-token>"};
  }

  // Create a namespace alias to make the code easier to read.
  namespace cbt = ::google::cloud::bigtable;
  using ::google::cloud::GrpcCredentialOption;
  using ::google::cloud::Options;
  using ::google::cloud::StatusOr;

  //! [test access token]
  [](std::string const& project_id, std::string const& instance_id,
     std::string const& access_token) {
    auto call_credentials = grpc::AccessTokenCredentials(access_token);
    auto channel_credentials =
        grpc::SslCredentials(grpc::SslCredentialsOptions());
    auto credentials = grpc::CompositeChannelCredentials(channel_credentials,
                                                         call_credentials);
    auto options = Options{}.set<GrpcCredentialOption>(credentials);

    cbt::TableAdmin admin(
        cbt::CreateDefaultAdminClient(project_id, cbt::ClientOptions(options)),
        instance_id);

    auto tables = admin.ListTables(cbt::TableAdmin::NAME_ONLY);
    if (!tables) throw std::runtime_error(tables.status().message());
  }
  //! [test access token]
  (argv.at(0), argv.at(1), argv.at(2));
}

void JWTAccessToken(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw Usage{
        "test-jwt-access-token <project-id> <instance-id> "
        "<service_account_file_json>"};
  }
  // Create a namespace alias to make the code easier to read.
  namespace cbt = ::google::cloud::bigtable;
  using ::google::cloud::GrpcCredentialOption;
  using ::google::cloud::Options;
  using ::google::cloud::StatusOr;

  //! [test jwt access token]
  [](std::string const& project_id, std::string const& instance_id,
     std::string const& service_account_file_json) {
    std::ifstream stream(service_account_file_json);
    if (!stream.is_open()) {
      std::ostringstream os;
      os << "JWTAccessToken(" << service_account_file_json
         << "): cannot open upload file source";
      throw std::runtime_error(os.str());
    }
    std::string json_key(std::istreambuf_iterator<char>{stream}, {});

    auto call_credentials =
        grpc::ServiceAccountJWTAccessCredentials(json_key, 6000);
    auto channel_credentials =
        grpc::SslCredentials(grpc::SslCredentialsOptions());
    auto credentials = grpc::CompositeChannelCredentials(channel_credentials,
                                                         call_credentials);
    auto options = Options{}.set<GrpcCredentialOption>(credentials);

    cbt::TableAdmin admin(
        cbt::CreateDefaultAdminClient(project_id, cbt::ClientOptions(options)),
        instance_id);

    auto tables = admin.ListTables(cbt::TableAdmin::NAME_ONLY);
    if (!tables) throw std::runtime_error(tables.status().message());
  }
  //! [test jwt access token]
  (argv.at(0), argv.at(1), argv.at(2));
}

void GCECredentials(std::vector<std::string> const& argv) {
  if (argv.size() != 2) {
    throw Usage{"test-gce-credentials: <project-id> <instance-id>"};
  }
  // Create a namespace alias to make the code easier to read.
  namespace cbt = ::google::cloud::bigtable;
  using ::google::cloud::GrpcCredentialOption;
  using ::google::cloud::Options;
  using ::google::cloud::StatusOr;

  //! [test gce credentials]
  [](std::string const& project_id, std::string const& instance_id) {
    auto call_credentials = grpc::GoogleComputeEngineCredentials();
    auto channel_credentials =
        grpc::SslCredentials(grpc::SslCredentialsOptions());
    auto credentials = grpc::CompositeChannelCredentials(channel_credentials,
                                                         call_credentials);
    auto options = Options{}.set<GrpcCredentialOption>(credentials);

    cbt::TableAdmin admin(
        cbt::CreateDefaultAdminClient(project_id, cbt::ClientOptions(options)),
        instance_id);

    auto tables = admin.ListTables(cbt::TableAdmin::NAME_ONLY);
    if (!tables) throw std::runtime_error(tables.status().message());
  }
  //! [test gce credentials]
  (argv.at(0), argv.at(1));
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::bigtable::examples;

  if (!argv.empty()) throw Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
      "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID",
      "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ACCESS_TOKEN",
      "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_KEY_FILE_JSON",
  });

  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto const instance_id = google::cloud::internal::GetEnv(
                               "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID")
                               .value();
  auto const access_token = google::cloud::internal::GetEnv(
                                "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ACCESS_TOKEN")
                                .value();
  auto const credentials_file =
      google::cloud::internal::GetEnv(
          "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_KEY_FILE_JSON")
          .value();

  AccessToken({project_id, instance_id, access_token});
  JWTAccessToken({project_id, instance_id, credentials_file});
}

}  // anonymous namespace

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InstallCrashHandler(argv[0]);

  google::cloud::bigtable::examples::Commands commands = {
      {"test-access-token", AccessToken},
      {"test-jwt-access-token", JWTAccessToken},
      {"test-gce-credentials", GCECredentials},
      {"auto", RunAll},
  };

  google::cloud::bigtable::examples::Example example(std::move(commands));
  return example.Run(argc, argv);
}
