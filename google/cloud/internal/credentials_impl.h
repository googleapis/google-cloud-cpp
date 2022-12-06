// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CREDENTIALS_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CREDENTIALS_IMPL_H

#include "google/cloud/credentials.h"
#include "google/cloud/internal/access_token.h"
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
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

class InsecureCredentialsConfig;
class GoogleDefaultCredentialsConfig;
class AccessTokenConfig;
class ImpersonateServiceAccountConfig;
class ServiceAccountConfig;
class ExternalAccountConfig;

class CredentialsVisitor {
 public:
  virtual ~CredentialsVisitor() = default;
  virtual void visit(InsecureCredentialsConfig&) = 0;
  virtual void visit(GoogleDefaultCredentialsConfig&) = 0;
  virtual void visit(AccessTokenConfig&) = 0;
  virtual void visit(ImpersonateServiceAccountConfig&) = 0;
  virtual void visit(ServiceAccountConfig&) = 0;
  virtual void visit(ExternalAccountConfig&) = 0;

  static void dispatch(Credentials& credentials, CredentialsVisitor& visitor);
};

class InsecureCredentialsConfig : public Credentials {
 public:
  explicit InsecureCredentialsConfig(Options opts);
  ~InsecureCredentialsConfig() override = default;

  Options const& options() const { return options_; }

 private:
  void dispatch(CredentialsVisitor& v) override { v.visit(*this); }

  Options options_;
};

class GoogleDefaultCredentialsConfig : public Credentials {
 public:
  explicit GoogleDefaultCredentialsConfig(Options opts);
  ~GoogleDefaultCredentialsConfig() override = default;

  Options const& options() const { return options_; }

 private:
  void dispatch(CredentialsVisitor& v) override { v.visit(*this); }

  Options options_;
};

class AccessTokenConfig : public Credentials {
 public:
  AccessTokenConfig(std::string token,
                    std::chrono::system_clock::time_point expiration,
                    Options opts);
  ~AccessTokenConfig() override = default;

  AccessToken const& access_token() const { return access_token_; }
  Options const& options() const { return options_; }

 private:
  void dispatch(CredentialsVisitor& v) override { v.visit(*this); }

  AccessToken access_token_;
  Options options_;
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
  std::chrono::seconds lifetime() const;
  std::vector<std::string> const& scopes() const;
  std::vector<std::string> const& delegates() const;

 private:
  void dispatch(CredentialsVisitor& v) override { v.visit(*this); }

  std::shared_ptr<Credentials> base_credentials_;
  std::string target_service_account_;
  Options options_;
};

class ServiceAccountConfig : public Credentials {
 public:
  ServiceAccountConfig(std::string json_object, Options opts);

  std::string const& json_object() const { return json_object_; }
  Options const& options() const { return options_; }

 private:
  void dispatch(CredentialsVisitor& v) override { v.visit(*this); }

  std::string json_object_;
  Options options_;
};

class ExternalAccountConfig : public Credentials {
 public:
  ExternalAccountConfig(std::string j, Options o);

  std::string const& json_object() const { return json_object_; }
  Options const& options() const { return options_; }

 private:
  void dispatch(CredentialsVisitor& v) override { v.visit(*this); }

  std::string json_object_;
  Options options_;
};

/// A helper function to initialize Auth options.
Options PopulateAuthOptions(Options options);

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CREDENTIALS_IMPL_H
