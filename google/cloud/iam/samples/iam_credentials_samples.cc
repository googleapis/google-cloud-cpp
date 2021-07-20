// Copyright 2021 Google LLC
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

#include "google/cloud/iam/iam_credentials_client.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/example_driver.h"

namespace {

void GenerateAccessToken(std::vector<std::string> const& argv) {
  if (argv.size() < 3) {
    throw google::cloud::testing_util::Usage(
        "generate-access-token <service-account-name> <lifetime-seconds> "
        "<scope>+");
  }
  //! [START iamcredentials_generate_access_token]
  //! [iamcredentials-generate-access-token]
  namespace iam = google::cloud::iam;
  [](std::string const& name, std::string const& lifetime_seconds,
     std::vector<std::string> const& scope) {
    iam::IAMCredentialsClient client(iam::MakeIAMCredentialsConnection());
    google::protobuf::Duration lifetime;
    lifetime.set_seconds(std::stoi(lifetime_seconds));
    auto response = client.GenerateAccessToken(name, {}, scope, lifetime);
    if (!response) throw std::runtime_error(response.status().message());
    std::cout << "Access Token successfully created: "
              << response->DebugString() << "\n";
  }
  //! [END iamcredentials_generate_access_token]
  //! [iamcredentials-generate-access-token]
  (argv.at(0), argv.at(1), {argv.begin() + 2, argv.end()});
}

void GenerateIdToken(std::vector<std::string> const& argv) {
  if (argv.size() < 3) {
    throw google::cloud::testing_util::Usage(
        "generate-id-token <service-account-name> <audience> <include-email> "
        "[<delegates>]*");
  }
  //! [START iamcredentials_generate_id_token]
  //! [iamcredentials-generate-id-token]
  namespace iam = google::cloud::iam;
  [](std::string const& name, std::string const& audience,
     std::string const& include_email,
     std::vector<std::string> const& delegates) {
    iam::IAMCredentialsClient client(iam::MakeIAMCredentialsConnection());
    auto response = client.GenerateIdToken(name, delegates, audience,
                                           (include_email == "true"));
    if (!response) throw std::runtime_error(response.status().message());
    std::cout << "Id Token successfully created: " << response->DebugString()
              << "\n";
  }
  //! [END iamcredentials_generate_id_token] [iamcredentials-generate-id-token]
  (argv.at(0), argv.at(1), argv.at(2), {argv.begin() + 3, argv.end()});
}

void SignBlob(std::vector<std::string> const& argv) {
  if (argv.size() < 2) {
    throw google::cloud::testing_util::Usage(
        "sign-blob <service-account-name> <payload> [<delegates>]*");
  }
  //! [START iamcredentials_sign_blob] [iamcredentials-sign-blob]
  namespace iam = google::cloud::iam;
  [](std::string const& name, std::string const& payload,
     std::vector<std::string> const& delegates) {
    iam::IAMCredentialsClient client(iam::MakeIAMCredentialsConnection());
    auto response = client.SignBlob(name, delegates, payload);
    if (!response) throw std::runtime_error(response.status().message());
    std::cout << "Blob successfully signed: " << response->DebugString()
              << "\n";
  }
  //! [END iamcredentials_sign_blob] [iamcredentials-sign-blob]
  (argv.at(0), argv.at(1), {argv.begin() + 2, argv.end()});
}

void SignJwt(std::vector<std::string> const& argv) {
  if (argv.size() < 2) {
    throw google::cloud::testing_util::Usage(
        "sign-jwt <service-account-name> <payload> [<delegates>]*");
  }
  //! [START iamcredentials_sign_jwt] [iamcredentials-sign-jwt]
  namespace iam = google::cloud::iam;
  [](std::string const& name, std::string const& payload,
     std::vector<std::string> const& delegates) {
    iam::IAMCredentialsClient client(iam::MakeIAMCredentialsConnection());
    auto response = client.SignJwt(name, delegates, payload);
    if (!response) throw std::runtime_error(response.status().message());
    std::cout << "JWT successfully signed: " << response->DebugString() << "\n";
  }
  //! [END iamcredentials_sign_jwt] [iamcredentials-sign-jwt]
  (argv.at(0), argv.at(1), {argv.begin() + 2, argv.end()});
}

void AutoRun(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
  });
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_CPP_IAM_CREDENTIALS_TEST_SERVICE_ACCOUNT",
  });
  auto const service_account_id =
      google::cloud::internal::GetEnv(
          "GOOGLE_CLOUD_CPP_IAM_CREDENTIALS_TEST_SERVICE_ACCOUNT")
          .value();
  auto const service_account_name =
      absl::StrCat("projects/-/serviceAccounts/", service_account_id);
  std::string const scope = "https://www.googleapis.com/auth/spanner.admin";
  std::string const blob_payload = "some_payload_bytes";
  std::string const json_payload = R"({"some": "json"})";

  GenerateAccessToken({service_account_name, "3600", scope});
  GenerateIdToken({service_account_name, scope, "true"});
  SignBlob({service_account_name, blob_payload});
  SignJwt({service_account_name, json_payload});

  std::cout << "\nAutoRun done" << std::endl;
}
}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  google::cloud::testing_util::Example example(
      {{"generate-access-token", GenerateAccessToken},
       {"generate-id-token", GenerateIdToken},
       {"sign-blob", SignBlob},
       {"sign-jwt", SignJwt},
       {"auto", AutoRun}});
  return example.Run(argc, argv);
}
