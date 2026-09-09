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

#include "google/cloud/internal/unified_grpc_credentials.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/credentials_impl.h"
#include "google/cloud/internal/filesystem.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>
#if __has_include(<grpcpp/version_info.h>)
#include <grpcpp/version_info.h>
#endif
#include <nlohmann/json.hpp>
#include <fstream>
#include <string_view>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Pair;

TEST(UnifiedGrpcCredentialsTest, GrpcCredentialOption) {
  CompletionQueue cq;
  auto result = CreateAuthenticationStrategy(
      cq,
      Options{}.set<GrpcCredentialOption>(grpc::InsecureChannelCredentials()));
  EXPECT_FALSE(result->RequiresConfigureContext());
}

TEST(UnifiedGrpcCredentialsTest, UnifiedCredentialsOption) {
  auto const expiration =
      std::chrono::system_clock::now() + std::chrono::hours(1);
  CompletionQueue cq;
  auto result = CreateAuthenticationStrategy(
      cq, Options{}
              .set<UnifiedCredentialsOption>(
                  MakeAccessTokenCredentials("test-token", expiration))
              .set<GrpcCredentialOption>(grpc::InsecureChannelCredentials()));
  // Verify `UnifiedCredentialsOption` is used before `GrpcCredentialOption`
  EXPECT_TRUE(result->RequiresConfigureContext());
}

TEST(UnifiedGrpcCredentialsTest, WithGrpcCredentials) {
  auto result =
      CreateAuthenticationStrategy(grpc::InsecureChannelCredentials());
  ASSERT_NE(nullptr, result.get());
  grpc::ClientContext context;
  auto status = result->ConfigureContext(context);
  ASSERT_EQ(nullptr, context.credentials());
}

TEST(UnifiedGrpcCredentialsTest, WithInsecureCredentials) {
  CompletionQueue cq;
  auto result = CreateAuthenticationStrategy(*MakeInsecureCredentials(), cq);
  ASSERT_NE(nullptr, result.get());
  grpc::ClientContext context;
  auto status = result->ConfigureContext(context);
  EXPECT_THAT(status, IsOk());
  ASSERT_EQ(nullptr, context.credentials());
}

TEST(UnifiedGrpcCredentialsTest, WithDefaultCredentials) {
  // Create a filename for a file that (most likely) does not exist. We just
  // want to initialize the default credentials, the filename won't be used by
  // the test.
  ScopedEnvironment env("GOOGLE_APPLICATION_CREDENTIALS", "unused.json");

  CompletionQueue cq;
  auto result =
      CreateAuthenticationStrategy(*MakeGoogleDefaultCredentials(), cq);
  ASSERT_NE(nullptr, result.get());
  grpc::ClientContext context;
  auto status = result->ConfigureContext(context);
  EXPECT_THAT(status, IsOk());
  ASSERT_EQ(nullptr, context.credentials());
}

TEST(UnifiedGrpcCredentialsTest, WithAccessTokenCredentials) {
  auto const expiration =
      std::chrono::system_clock::now() + std::chrono::hours(1);
  CompletionQueue cq;
  auto result = CreateAuthenticationStrategy(
      *MakeAccessTokenCredentials("test-token", expiration), cq);
  ASSERT_NE(nullptr, result.get());
  grpc::ClientContext context;
  auto status = result->ConfigureContext(context);
  EXPECT_THAT(status, IsOk());
  ASSERT_NE(nullptr, context.credentials());
}

TEST(UnifiedGrpcCredentialsTest, WithErrorCredentials) {
  Status const error_status{StatusCode::kFailedPrecondition,
                            "Precondition failed."};
  CompletionQueue cq;
  auto result = CreateAuthenticationStrategy(
      *internal::MakeErrorCredentials(error_status), cq);
  ASSERT_NE(nullptr, result.get());
  EXPECT_TRUE(result->RequiresConfigureContext());
  grpc::ClientContext context;
  auto configured_context = result->ConfigureContext(context);
  EXPECT_THAT(configured_context, StatusIs(error_status.code()));
  auto async_configured_context =
      result->AsyncConfigureContext(std::make_shared<grpc::ClientContext>())
          .get();
  EXPECT_THAT(async_configured_context, StatusIs(error_status.code()));
  auto channel = result->CreateChannel(std::string{}, grpc::ChannelArguments{});
  EXPECT_THAT(channel.get(), Not(IsNull()));
}

TEST(UnifiedGrpcCredentialsTest, WithApiKeyCredentials) {
  CompletionQueue cq;
  auto creds = ApiKeyConfig("api-key", Options{});
  auto auth = CreateAuthenticationStrategy(creds, cq);
  ASSERT_THAT(auth, NotNull());

  grpc::ClientContext context;
  auto status = auth->ConfigureContext(context);
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(context.credentials(), IsNull());

  ValidateMetadataFixture fixture;
  auto headers = fixture.GetMetadata(context);
  EXPECT_THAT(headers, Contains(Pair("x-goog-api-key", "api-key")));
}

TEST(UnifiedGrpcCredentialsTest, WithAuthorizedUserCredentials) {
  CompletionQueue cq;
  auto creds = AuthorizedUserConfig("{}", Options{});
  auto auth = CreateAuthenticationStrategy(creds, cq);
  ASSERT_THAT(auth, NotNull());
  EXPECT_TRUE(auth->RequiresConfigureContext());

  grpc::ClientContext context;
  auto configured_context = auth->ConfigureContext(context);
  EXPECT_THAT(configured_context, StatusIs(StatusCode::kUnimplemented));
}

TEST(UnifiedGrpcCredentialsTest, LoadCAInfoNotSet) {
  auto contents = LoadCAInfo(Options{});
  EXPECT_FALSE(contents.has_value());
}

std::string CreateRandomFileName() {
  static DefaultPRNG generator = MakeDefaultPRNG();
  // When running on the internal Google CI systems we cannot write to the local
  // directory, GTest has a good temporary directory in that case.
  return google::cloud::internal::PathAppend(
      ::testing::TempDir(),
      Sample(generator, 8, "abcdefghijklmnopqrstuvwxyz0123456789"));
}

TEST(UnifiedGrpcCredentialsTest, LoadCAInfoNotExist) {
  auto filename = CreateRandomFileName();
  auto contents = LoadCAInfo(Options{}.set<CARootsFilePathOption>(filename));
  ASSERT_TRUE(contents.has_value());
  EXPECT_THAT(*contents, IsEmpty());
}

TEST(UnifiedGrpcCredentialsTest, LoadCAInfoContents) {
  auto filename = CreateRandomFileName();
  auto const expected =
      std::string{"The quick brown fox jumps over the lazy dog"};
  std::ofstream(filename) << expected;
  auto contents = LoadCAInfo(Options{}.set<CARootsFilePathOption>(filename));
  (void)std::remove(filename.c_str());  // remove the temporary file
  ASSERT_TRUE(contents.has_value());
  EXPECT_EQ(*contents, expected);
}

auto constexpr kWellFormattedECKey = R"""(-----BEGIN EC PRIVATE KEY-----
MHcCAQEEIDGD4hNeIBG3lo4BKS1k4jpYhbnJSZwAuUwyK8wEiOP5oAoGCCqGSM49
AwEHoUQDQgAEWK7gDAGAAzOfl6pHhpmvjbTeUPyclQk7+HgAWE6uGUtox/U8/sQQ
X3IM7YomoAWiNKWwBVskpXWj7L9dLkqhyQ==
-----END EC PRIVATE KEY-----
)""";

std::string MakeGDCHServiceAccountContents(std::string_view ca_cert_path = "") {
  nlohmann::json j{
      {"type", "gdch_service_account"},
      {"format_version", "1"},
      {"project", "test-project"},
      {"private_key_id", "test-private-key-id"},
      {"private_key", kWellFormattedECKey},
      {"name", "test-name"},
      {"token_uri", "https://test-token-uri.com/token"},
  };
  if (!ca_cert_path.empty()) {
    j["ca_cert_path"] = ca_cert_path;
  }
  return j.dump();
}

TEST(UnifiedGrpcCredentialsTest, WithGDCHServiceAccountCredentialsFromJson) {
#if !defined(GRPC_CPP_VERSION_MAJOR) || \
    (GRPC_CPP_VERSION_MAJOR < 1 ||      \
     (GRPC_CPP_VERSION_MAJOR == 1 && GRPC_CPP_VERSION_MINOR < 84))
  GTEST_SKIP() << "GDCH credentials require gRPC >= 1.84.0";
#endif
  CompletionQueue cq;
  std::string json_contents = MakeGDCHServiceAccountContents();
  std::shared_ptr<Credentials> creds = MakeGDCHServiceAccountCredentials(
      json_contents, "https://my-audience.com");
  std::shared_ptr<GrpcAuthenticationStrategy> auth =
      CreateAuthenticationStrategy(*creds, cq);
  ASSERT_THAT(auth, NotNull());

  grpc::ClientContext context;
  Status status = auth->ConfigureContext(context);
  EXPECT_THAT(status, IsOk());
  EXPECT_THAT(context.credentials(), IsNull());
}

TEST(UnifiedGrpcCredentialsTest, WithGDCHServiceAccountCredentialsFromFile) {
#if !defined(GRPC_CPP_VERSION_MAJOR) || \
    (GRPC_CPP_VERSION_MAJOR < 1 ||      \
     (GRPC_CPP_VERSION_MAJOR == 1 && GRPC_CPP_VERSION_MINOR < 84))
  GTEST_SKIP() << "GDCH credentials require gRPC >= 1.84.0";
#endif
  std::string filename = CreateRandomFileName();
  std::ofstream(filename) << MakeGDCHServiceAccountContents();
  ScopedEnvironment env("GOOGLE_APPLICATION_CREDENTIALS", filename);

  CompletionQueue cq;
  std::shared_ptr<Credentials> creds =
      MakeGDCHServiceAccountCredentials("https://my-audience.com");
  std::shared_ptr<GrpcAuthenticationStrategy> auth =
      CreateAuthenticationStrategy(*creds, cq);
  (void)std::remove(filename.c_str());
  ASSERT_THAT(auth, NotNull());

  grpc::ClientContext context;
  Status status = auth->ConfigureContext(context);
  EXPECT_THAT(status, IsOk());
  EXPECT_THAT(context.credentials(), IsNull());
}

TEST(UnifiedGrpcCredentialsTest, WithGDCHServiceAccountCredentialsMissingFile) {
#if !defined(GRPC_CPP_VERSION_MAJOR) || \
    (GRPC_CPP_VERSION_MAJOR < 1 ||      \
     (GRPC_CPP_VERSION_MAJOR == 1 && GRPC_CPP_VERSION_MINOR < 84))
  GTEST_SKIP() << "GDCH credentials require gRPC >= 1.84.0";
#endif
  std::string filename = CreateRandomFileName();
  ScopedEnvironment env("GOOGLE_APPLICATION_CREDENTIALS", filename);

  CompletionQueue cq;
  std::shared_ptr<Credentials> creds =
      MakeGDCHServiceAccountCredentials("https://my-audience.com");
  std::shared_ptr<GrpcAuthenticationStrategy> auth =
      CreateAuthenticationStrategy(*creds, cq);
  ASSERT_THAT(auth, NotNull());

  grpc::ClientContext context;
  Status status = auth->ConfigureContext(context);
  EXPECT_THAT(status, StatusIs(StatusCode::kUnknown));
}

TEST(UnifiedGrpcCredentialsTest, WithGDCHServiceAccountCredentialsInvalidJson) {
#if !defined(GRPC_CPP_VERSION_MAJOR) || \
    (GRPC_CPP_VERSION_MAJOR < 1 ||      \
     (GRPC_CPP_VERSION_MAJOR == 1 && GRPC_CPP_VERSION_MINOR < 84))
  GTEST_SKIP() << "GDCH credentials require gRPC >= 1.84.0";
#endif
  CompletionQueue cq;
  std::shared_ptr<Credentials> creds = MakeGDCHServiceAccountCredentials(
      "invalid json", "https://my-audience.com");
  std::shared_ptr<GrpcAuthenticationStrategy> auth =
      CreateAuthenticationStrategy(*creds, cq);
  ASSERT_THAT(auth, NotNull());

  grpc::ClientContext context;
  Status status = auth->ConfigureContext(context);
  EXPECT_THAT(status, StatusIs(StatusCode::kInternal));
}

auto constexpr kCACertificate = R"""(
-----BEGIN CERTIFICATE-----
MIIEPTCCAyWgAwIBAgIUXa/2HsbYrolo1Cox/1SOqLDnNYQwDQYJKoZIhvcNAQEL
BQAwga0xCzAJBgNVBAYTAlVTMREwDwYDVQQIDAhOZXcgWW9yazERMA8GA1UEBwwI
TmV3IFlvcmsxGjAYBgNVBAoMEVRlc3QtT25seSBJbnZhbGlkMRIwEAYDVQQLDAlU
ZXN0LU9ubHkxGjAYBgNVBAMMEVRlc3QtT25seSBJbnZhbGlkMSwwKgYJKoZIhvcN
AQkBFh10ZXN0LW9ubHlAaW52YWxpZC5leGFtcGxlLmNvbTAeFw0yMTA1MjUxOTQy
MTdaFw0zMTA1MjMxOTQyMTdaMIGtMQswCQYDVQQGEwJVUzERMA8GA1UECAwITmV3
IFlvcmsxETAPBgNVBAcMCE5ldyBZb3JrMRowGAYDVQQKDBFUZXN0LU9ubHkgSW52
YWxpZDESMBAGA1UECwwJVGVzdC1Pbmx5MRowGAYDVQQDDBFUZXN0LU9ubHkgSW52
YWxpZDEsMCoGCSqGSIb3DQEJARYddGVzdC1vbmx5QGludmFsaWQuZXhhbXBsZS5j
b20wggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDL66K2V2OQHcb2Ab7o
ucWqb3iOF1IGvc6lzC2XeqrqCvYF5HB9jK+cWHDmeGjJMoYI35S2fp+Wzh3Qek0Y
ilQBylUq91y2ZnqAmFu7gc83wWgWPHCkKPKTS5tcK5sQTbhBuaQBQs5hWCeNfZOy
AtAU2ysNde79DwSXfq/e8NLRvaKsS8etqiLyfIuDWfXHzIDgAgyyi49m67fYnLYx
y2W555Zh0vAnd4MQYh0SYQ64BgaUg59WLRYhsHN5r9D06JEur9uxMLmtiQy7UyL2
xuw6u2y/+vcdTb9L9zaNEnITPl+3N23TG5KgBxLfKkHzNE7gS/w7ljS0ljNExuUO
UZutAgMBAAGjUzBRMB0GA1UdDgQWBBS7T7DuKDv1Hz1e693kY7gLXSu8PjAfBgNV
HSMEGDAWgBS7T7DuKDv1Hz1e693kY7gLXSu8PjAPBgNVHRMBAf8EBTADAQH/MA0G
CSqGSIb3DQEBCwUAA4IBAQC/4HmQwp7KjKF5FEnlHG5Chqob/yiPkwbMDV1yKA+i
ZMknQFgm8h0X4kLAbOiao71MPI5Zi5OQge6GoSXJJQYkesgdakPw6xkFfz9MsfCp
zMKm7sKIIGNaPMMqMJJ/cciCRIzXKl6gcOkZLlIFU1T2uE764Lc08yXIY7eXkVo1
8w4Gv6nH7uaQiESa2wt3TWGISG9wdDkcVG01tTr4jzq77yWuC1Ela2UvQK7AIWbK
OB6Sby1bvjYv0kqjinu9AcSCHUHJ1sJaPD5DvAyKP7W01YedP4iqLxkTlIRjaikD
KlXA1yQW/ClmnHVg57SN1g1rvOJCcnHBnSbT7kGFqUol
-----END CERTIFICATE-----
)""";

TEST(UnifiedGrpcCredentialsTest,
     WithGDCHServiceAccountCredentialsCustomCACertPath) {
#if !defined(GRPC_CPP_VERSION_MAJOR) || \
    (GRPC_CPP_VERSION_MAJOR < 1 ||      \
     (GRPC_CPP_VERSION_MAJOR == 1 && GRPC_CPP_VERSION_MINOR < 84))
  GTEST_SKIP() << "GDCH credentials require gRPC >= 1.84.0";
#endif
  std::string ca_filename = CreateRandomFileName();
  std::ofstream(ca_filename) << kCACertificate;

  std::string json_contents = MakeGDCHServiceAccountContents(ca_filename);
  CompletionQueue cq;
  std::shared_ptr<Credentials> creds = MakeGDCHServiceAccountCredentials(
      json_contents, "https://my-audience.com");
  std::shared_ptr<GrpcAuthenticationStrategy> auth =
      CreateAuthenticationStrategy(*creds, cq);
  (void)std::remove(ca_filename.c_str());
  ASSERT_THAT(auth, NotNull());

  grpc::ClientContext context;
  Status status = auth->ConfigureContext(context);
  EXPECT_THAT(status, IsOk());
  EXPECT_THAT(context.credentials(), IsNull());
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
