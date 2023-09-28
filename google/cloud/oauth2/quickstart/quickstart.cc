// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! [all]
#include "google/cloud/oauth2/access_token_generator.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 1) {
    std::cerr << "Usage: " << argv[0] << "\n";
    return 1;
  }

  // Use Application Default Credentials to initialize an AccessTokenGenerator
  // retrieve the access token.
  auto credentials = google::cloud::MakeGoogleDefaultCredentials();
  auto generator =
      google::cloud::oauth2::MakeAccessTokenGenerator(*credentials);
  auto token = generator->GetToken();
  if (!token) throw std::move(token).status();
  // Only print the first few characters to avoid leaking tokens in test logs.
  std::cout << "The access token starts with " << token->token.substr(0, 16)
            << "\n";

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
//! [all]
