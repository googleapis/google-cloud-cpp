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

#include "google/cloud/internal/populate_rest_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/credentials_impl.h"
#include "google/cloud/internal/rest_options.h"
#include "google/cloud/opentelemetry_options.h"
#include "absl/types/optional.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

struct Visitor : public CredentialsVisitor {
  std::string name;
  Options options;

  void visit(ErrorCredentialsConfig const&) override {
    name = "ErrorCredentialsConfig";
  }
  void visit(InsecureCredentialsConfig const&) override {
    name = "InsecureCredentialsConfig";
  }
  void visit(GoogleDefaultCredentialsConfig const& cfg) override {
    name = "GoogleDefaultCredentialsConfig";
    options = cfg.options();
  }
  void visit(AccessTokenConfig const&) override { name = "AccessTokenConfig"; }
  void visit(ImpersonateServiceAccountConfig const&) override {
    name = "ImpersonateServiceAccountConfig";
  }
  void visit(ServiceAccountConfig const&) override {
    name = "ServiceAccountConfig";
  }
  void visit(ExternalAccountConfig const&) override {
    name = "ExternalAccountConfig";
  }
};

TEST(PopulateRestOptions, EndpointOption) {
  struct TestCase {
    std::string input;
    std::string output;
  };
  std::vector<TestCase> cases = {
      {"example.com", "https://example.com"},
      {"http://example.com", "http://example.com"},
      {"https://example.com", "https://example.com"}};

  for (auto const& c : cases) {
    auto options = PopulateRestOptions(Options{}.set<EndpointOption>(c.input));
    EXPECT_EQ(options.get<EndpointOption>(), c.output);
  }
}

TEST(PopulateRestOptions, EmptyCredentials) {
  auto options = Options{};
  options = PopulateRestOptions(std::move(options));
  auto const& creds = options.get<UnifiedCredentialsOption>();

  Visitor v;
  CredentialsVisitor::dispatch(*creds, v);
  EXPECT_EQ(v.name, "GoogleDefaultCredentialsConfig");
}

TEST(PopulateRestOptions, EmptyCredentialsUsesAuthOptions) {
  auto options =
      Options{}.set<EndpointOption>("ignored").set<OpenTelemetryTracingOption>(
          true);
  options = PopulateRestOptions(std::move(options));
  auto const& creds = options.get<UnifiedCredentialsOption>();

  Visitor v;
  CredentialsVisitor::dispatch(*creds, v);
  EXPECT_EQ(v.name, "GoogleDefaultCredentialsConfig");
  EXPECT_FALSE(v.options.has<EndpointOption>());
  EXPECT_TRUE(v.options.get<OpenTelemetryTracingOption>());
}

TEST(PopulateRestOptions, CustomCredentials) {
  auto const expiration = std::chrono::system_clock::now();
  auto creds = MakeAccessTokenCredentials("access-token", expiration);
  auto options =
      PopulateRestOptions(Options{}.set<UnifiedCredentialsOption>(creds));
  EXPECT_EQ(options.get<UnifiedCredentialsOption>(), creds);
}

TEST(PopulateRestOptions, LongrunningEndpointOption) {
  using ::google::cloud::rest_internal::LongrunningEndpointOption;
  auto options = Options{};
  options = PopulateRestOptions(std::move(options));
  EXPECT_EQ(options.get<LongrunningEndpointOption>(),
            "https://longrunning.googleapis.com");

  options =
      Options{}.set<LongrunningEndpointOption>("https://foo.googleapis.com");
  options = PopulateRestOptions(std::move(options));
  EXPECT_EQ(options.get<LongrunningEndpointOption>(),
            "https://foo.googleapis.com");
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
