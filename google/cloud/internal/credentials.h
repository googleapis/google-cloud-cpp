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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CREDENTIALS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CREDENTIALS_H

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
class CredentialsVisitor;
}  // namespace internal

// TODO(#6293) - move unified credentials out of internal namespace
namespace internal {
class Credentials {
 public:
  virtual ~Credentials() = 0;

 private:
  friend class CredentialsVisitor;
  virtual void dispatch(CredentialsVisitor& visitor) = 0;
};

std::shared_ptr<Credentials> GoogleDefaultCredentials();
std::shared_ptr<Credentials> AccessTokenCredentials(
    std::string const& access_token,
    std::chrono::system_clock::time_point expiration);

/// A wrapper to store credentials into an options
struct UnifiedCredentialsOption {
  using Type = std::shared_ptr<Credentials>;
};

}  // namespace internal

namespace internal {

/// Represents an access token with a known expiration time.
struct AccessToken {
  std::string token;
  std::chrono::system_clock::time_point expiration;
};

/// A generator for access tokens.
using AccessTokenSource = std::function<AccessToken()>;

/// A generic generator of access tokens.
std::shared_ptr<Credentials> DynamicAccessTokenCredentials(
    AccessTokenSource source);

class GoogleDefaultCredentialsConfig;
class DynamicAccessTokenConfig;

class CredentialsVisitor {
 public:
  virtual ~CredentialsVisitor() = default;
  virtual void visit(GoogleDefaultCredentialsConfig&) = 0;
  virtual void visit(DynamicAccessTokenConfig&) = 0;

  static void dispatch(Credentials& credentials, CredentialsVisitor& visitor);
};

class GoogleDefaultCredentialsConfig : public Credentials {
 public:
  ~GoogleDefaultCredentialsConfig() override = default;

 private:
  void dispatch(CredentialsVisitor& v) override { v.visit(*this); }
};

class DynamicAccessTokenConfig : public Credentials {
 public:
  explicit DynamicAccessTokenConfig(AccessTokenSource source)
      : source_(std::move(source)) {}

  AccessTokenSource source() const { return source_; }

 private:
  void dispatch(CredentialsVisitor& v) override { v.visit(*this); }
  AccessTokenSource source_;
};

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CREDENTIALS_H
