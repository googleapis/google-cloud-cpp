// Copyright 2023 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_CREDENTIALS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_CREDENTIALS_H

#include "google/cloud/access_token.h"
#include "google/cloud/internal/credentials_impl.h"
#include "google/cloud/options.h"
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace testing_util {

struct TestCredentialsVisitor : public internal::CredentialsVisitor {
  std::string name;
  AccessToken access_token;
  internal::ImpersonateServiceAccountConfig const* impersonate = nullptr;
  std::string json_object;
  std::string api_key;
  Options options;

  void visit(internal::ErrorCredentialsConfig const&) override {
    name = "ErrorCredentialsConfig";
  }
  void visit(internal::InsecureCredentialsConfig const&) override {
    name = "InsecureCredentialsConfig";
  }
  void visit(internal::GoogleDefaultCredentialsConfig const& cfg) override {
    name = "GoogleDefaultCredentialsConfig";
    options = cfg.options();
  }
  void visit(internal::AccessTokenConfig const& cfg) override {
    name = "AccessTokenConfig";
    access_token = cfg.access_token();
  }
  void visit(internal::ImpersonateServiceAccountConfig const& cfg) override {
    name = "ImpersonateServiceAccountConfig";
    impersonate = &cfg;
  }
  void visit(internal::ServiceAccountConfig const& cfg) override {
    name = "ServiceAccountConfig";
    json_object = cfg.json_object();
  }
  void visit(internal::ExternalAccountConfig const& cfg) override {
    name = "ExternalAccountConfig";
    json_object = cfg.json_object();
    options = cfg.options();
  }
  void visit(internal::ApiKeyConfig const& cfg) override {
    name = "ApiKeyConfig";
    api_key = cfg.api_key();
  }
};

}  // namespace testing_util
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_CREDENTIALS_H
