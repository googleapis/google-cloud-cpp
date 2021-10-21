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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CREDENTIALS_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CREDENTIALS_IMPL_H

#include "google/cloud/credentials.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

/// Represents an access token with a known expiration time.
struct AccessToken {
  std::string token;
  std::chrono::system_clock::time_point expiration;
};

class InsecureCredentialsConfig;
class GoogleDefaultCredentialsConfig;
class AccessTokenConfig;
class ImpersonateServiceAccountConfig;
class ServiceAccountConfig;

class CredentialsVisitor {
 public:
  virtual ~CredentialsVisitor() = default;
  virtual void visit(InsecureCredentialsConfig&) = 0;
  virtual void visit(GoogleDefaultCredentialsConfig&) = 0;
  virtual void visit(AccessTokenConfig&) = 0;
  virtual void visit(ImpersonateServiceAccountConfig&) = 0;
  virtual void visit(ServiceAccountConfig&) = 0;

  static void dispatch(Credentials& credentials, CredentialsVisitor& visitor);
};

class InsecureCredentialsConfig : public Credentials {
 public:
  ~InsecureCredentialsConfig() override = default;

 private:
  void dispatch(CredentialsVisitor& v) override { v.visit(*this); }
};

class GoogleDefaultCredentialsConfig : public Credentials {
 public:
  ~GoogleDefaultCredentialsConfig() override = default;

 private:
  void dispatch(CredentialsVisitor& v) override { v.visit(*this); }
};

class AccessTokenConfig : public Credentials {
 public:
  AccessTokenConfig(std::string token,
                    std::chrono::system_clock::time_point expiration)
      : access_token_(AccessToken{std::move(token), expiration}) {}
  ~AccessTokenConfig() override = default;

  AccessToken const& access_token() const { return access_token_; }

 private:
  void dispatch(CredentialsVisitor& v) override { v.visit(*this); }
  AccessToken access_token_;
};

class ImpersonateServiceAccountConfig : public Credentials {
 public:
  ImpersonateServiceAccountConfig(std::shared_ptr<Credentials> base_credentials,
                                  std::string target_service_account,
                                  Options opts);

  std::shared_ptr<Credentials> base_credentials() const {
    return base_credentials_;
  }
  std::string const& target_service_account() const {
    return target_service_account_;
  }
  std::chrono::seconds lifetime() const { return lifetime_; }
  std::vector<std::string> const& scopes() const { return scopes_; }
  std::vector<std::string> const& delegates() const { return delegates_; }

 private:
  void dispatch(CredentialsVisitor& v) override { v.visit(*this); }

  std::shared_ptr<Credentials> base_credentials_;
  std::string target_service_account_;
  std::chrono::seconds lifetime_;
  std::vector<std::string> scopes_;
  std::vector<std::string> delegates_;
};

class ServiceAccountConfig : public Credentials {
 public:
  explicit ServiceAccountConfig(std::string json_object)
      : json_object_(std::move(json_object)) {}

  std::string const& json_object() const& { return json_object_; }
  std::string&& json_object() && { return std::move(json_object_); }

 private:
  void dispatch(CredentialsVisitor& v) override { v.visit(*this); }

  std::string json_object_;
};

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CREDENTIALS_IMPL_H
