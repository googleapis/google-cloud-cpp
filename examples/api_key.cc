// Copyright 2024 Google LLC
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

// The #includes (with the extra blank line) are part of the code extracted into
// the reference documentation. We want to highlight what includes are needed.
// [START apikeys_create_api_key]
#include "google/cloud/apikeys/v2/api_keys_client.h"
#include "google/cloud/location.h"

// [END apikeys_create_api_key]
// [START apikeys_authenticate_api_key]
#include "google/cloud/language/v1/language_client.h"
#include "google/cloud/credentials.h"
#include "google/cloud/options.h"

// [END apikeys_authenticate_api_key]
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/example_driver.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

// [START apikeys_create_api_key]
google::api::apikeys::v2::Key CreateApiKey(
    google::cloud::apikeys_v2::ApiKeysClient client,
    google::cloud::Location location, std::string display_name) {
  google::api::apikeys::v2::CreateKeyRequest request;
  request.set_parent(location.FullName());
  request.mutable_key()->set_display_name(std::move(display_name));
  // As an example, restrict the API key's scope to the Natural Language API.
  request.mutable_key()->mutable_restrictions()->add_api_targets()->set_service(
      "language.googleapis.com");

  // Create the key, blocking on the result.
  auto key = client.CreateKey(request).get();
  if (!key) throw std::move(key.status());
  std::cout << "Successfully created an API key: " << key->name() << "\n";

  // For authenticating with the API key, use the value in `key->key_string()`.

  // The API key's resource name is the value in `key->name()`. Use this to
  // refer to the specific key in a `GetKey()` or `DeleteKey()` RPC.
  return *key;
}
// [END apikeys_create_api_key]

void CreateApiKeyCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 2) {
    throw google::cloud::testing_util::Usage{
        "create-api-key <project-id> <display-name>"};
  }
  auto client = google::cloud::apikeys_v2::ApiKeysClient(
      google::cloud::apikeys_v2::MakeApiKeysConnection());

  (void)CreateApiKey(client, google::cloud::Location(argv.at(0), "global"),
                     argv.at(1));
}

// [START apikeys_authenticate_api_key]
void AuthenticateWithApiKey(std::vector<std::string> const& argv) {
  if (argv.size() != 2) {
    throw google::cloud::testing_util::Usage{
        "authenticate-with-api-key <project-id> <api-key>"};
  }
  namespace gc = ::google::cloud;
  auto options = gc::Options{}.set<gc::UnifiedCredentialsOption>(
      gc::MakeApiKeyCredentials(argv[1]));
  auto client = gc::language_v1::LanguageServiceClient(
      gc::language_v1::MakeLanguageServiceConnection(options));

  auto constexpr kText = "Hello, world!";

  google::cloud::language::v1::Document d;
  d.set_content(kText);
  d.set_type(google::cloud::language::v1::Document::PLAIN_TEXT);

  auto response = client.AnalyzeSentiment(d, {});
  if (!response) throw std::move(response.status());
  auto const& sentiment = response->document_sentiment();
  std::cout << "Text: " << kText << "\n";
  std::cout << "Sentiment: " << sentiment.score() << ", "
            << sentiment.magnitude() << "\n";
  std::cout << "Successfully authenticated using the API key\n";
}
// [END apikeys_authenticate_api_key]

void AutoRun(std::vector<std::string> const& argv) {
  namespace gc = ::google::cloud;
  namespace examples = ::google::cloud::testing_util;
  using ::google::cloud::internal::GetEnv;
  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
  });
  auto const project_id = GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto const location = gc::Location(project_id, "global");

  auto constexpr kKeyPrefix = "examples/api_key.cc ";
  char delimiter = '@';
  auto options = gc::Options{}.set<gc::UserProjectOption>(project_id);
  auto client = gc::apikeys_v2::ApiKeysClient(
      gc::apikeys_v2::MakeApiKeysConnection(options));

  std::cout << "Cleaning up stale keys\n";
  auto stale = gc::internal::FormatUtcDate(std::chrono::system_clock::now() -
                                           std::chrono::hours(48));
  for (auto key : client.ListKeys(location.FullName())) {
    if (!key) throw std::move(key.status());
    std::vector<std::string> parts =
        absl::StrSplit(key->display_name(), delimiter);
    if (parts.size() == 2 && parts[0] == kKeyPrefix && parts[1] <= stale) {
      std::cout << "Deleting stale API Key: " << key->display_name() << "\n";
      (void)client.DeleteKey(key->name()).get();
    }
  }

  std::cout << "Running CreateApiKey\n";
  auto suffix = gc::internal::FormatUtcDate(std::chrono::system_clock::now());
  auto display_name = std::string(kKeyPrefix) + delimiter + suffix;
  auto key = CreateApiKey(client, location, std::move(display_name));

  std::cout << "Running AuthenticateWithApiKey\n";
  for (auto backoff : {60, 60, 60, 0}) {
    // API keys are not always usable immediately after they are created. To
    // give the API key some time to propagate, we implement a retry loop around
    // the RPC that attempts to authenticate with the newly created API key.

    // When we authenticate with an API key, we do not have (or need)
    // credentials. Using a quota project requires credentials, so disable it.
    google::cloud::testing_util::ScopedEnvironment overlay(
        "GOOGLE_CLOUD_CPP_USER_PROJECT", absl::nullopt);

    try {
      AuthenticateWithApiKey({project_id, key.key_string()});
      break;
    } catch (gc::Status const& s) {
      if (backoff == 0) throw(s);
      std::cout << "Sleeping for " << backoff << " seconds\n";
      std::this_thread::sleep_for(std::chrono::seconds(backoff));
    }
  }

  std::cout << "Deleting API Key\n";
  (void)client.DeleteKey(key.name()).get();
}

}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  google::cloud::testing_util::Example example({
      {"create-api-key", CreateApiKeyCommand},
      {"authenticate-with-api-key", AuthenticateWithApiKey},
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}
