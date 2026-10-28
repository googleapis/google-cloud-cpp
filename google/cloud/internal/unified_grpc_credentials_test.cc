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
#include "google/cloud/internal/background_threads_impl.h"
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
#include <algorithm>
#include <chrono>
#include <fstream>
#include <optional>
#include <string>
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
using ::testing::HasSubstr;
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

TEST(UnifiedGrpcCredentialsTest, WithExternalAccountCredentialsInvalid) {
  CompletionQueue cq;
  auto creds = ExternalAccountConfig("{}", Options{});
  auto auth = CreateAuthenticationStrategy(creds, cq);
  ASSERT_THAT(auth, NotNull());
  EXPECT_TRUE(auth->RequiresConfigureContext());

  grpc::ClientContext context;
  auto configured_context = auth->ConfigureContext(context);
  EXPECT_THAT(configured_context, StatusIs(StatusCode::kInvalidArgument));
}

TEST(UnifiedGrpcCredentialsTest, WithExternalAccountAuthorizedUserCredentials) {
  CompletionQueue cq;
  std::string const json = R"({
    "type": "external_account_authorized_user",
    "client_id": "test-client-id",
    "client_secret": "test-client-secret",
    "refresh_token": "test-refresh-token",
    "token_url": "https://sts.googleapis.com/v1/oauthtoken"
  })";
  auto creds = ExternalAccountConfig(json, Options{});
  auto auth = CreateAuthenticationStrategy(creds, cq);
  ASSERT_THAT(auth, NotNull());
  EXPECT_TRUE(auth->RequiresConfigureContext());

  grpc::ClientContext context;
  auto configured_context = auth->ConfigureContext(context);
  EXPECT_THAT(configured_context, StatusIs(StatusCode::kInvalidArgument));
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

std::string MakeGDCHServiceAccountContents(std::string_view ca_cert_path) {
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

std::string MakeGDCHServiceAccountContents() {
  return MakeGDCHServiceAccountContents("");
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

TEST(UnifiedGrpcCredentialsTest,
     WithGDCHServiceAccountCredentialsMissingCACertFile) {
#if !defined(GRPC_CPP_VERSION_MAJOR) || \
    (GRPC_CPP_VERSION_MAJOR < 1 ||      \
     (GRPC_CPP_VERSION_MAJOR == 1 && GRPC_CPP_VERSION_MINOR < 84))
  GTEST_SKIP() << "GDCH credentials require gRPC >= 1.84.0";
#endif
  std::string ca_filename = CreateRandomFileName();
  std::string json_contents = MakeGDCHServiceAccountContents(ca_filename);
  CompletionQueue cq;
  std::shared_ptr<Credentials> creds = MakeGDCHServiceAccountCredentials(
      json_contents, "https://my-audience.com");
  std::shared_ptr<GrpcAuthenticationStrategy> auth =
      CreateAuthenticationStrategy(*creds, cq);
  ASSERT_THAT(auth, NotNull());

  grpc::ClientContext context;
  Status status = auth->ConfigureContext(context);
  EXPECT_THAT(status, StatusIs(StatusCode::kUnknown,
                               HasSubstr("Cannot open CA certificate file")));
}

// The private key in this JSON object was invalidated (deleted) a long time
// ago. The service account and key it was created for were deleted too, and do
// not match the attributes listed here. gRPC parses the key when creating
// `ServiceAccountJWTAccessCredentials`, so it must be well-formed.
constexpr std::string_view kServiceAccountContents = R"""({
      "type": "service_account",
      "project_id": "foo-project",
      "private_key_id": "a1a111aa1111a11a11a11aa111a111a1a1111111",
      "private_key": "-----BEGIN PRIVATE KEY-----\nMIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCltiF2oP3KJJ+S\ntTc1McylY+TuAi3AdohX7mmqIjd8a3eBYDHs7FlnUrFC4CRijCr0rUqYfg2pmk4a\n6TaKbQRAhWDJ7XD931g7EBvCtd8+JQBNWVKnP9ByJUaO0hWVniM50KTsWtyX3up/\nfS0W2R8Cyx4yvasE8QHH8gnNGtr94iiORDC7De2BwHi/iU8FxMVJAIyDLNfyk0hN\neheYKfIDBgJV2v6VaCOGWaZyEuD0FJ6wFeLybFBwibrLIBE5Y/StCrZoVZ5LocFP\nT4o8kT7bU6yonudSCyNMedYmqHj/iF8B2UN1WrYx8zvoDqZk0nxIglmEYKn/6U7U\ngyETGcW9AgMBAAECggEAC231vmkpwA7JG9UYbviVmSW79UecsLzsOAZnbtbn1VLT\nPg7sup7tprD/LXHoyIxK7S/jqINvPU65iuUhgCg3Rhz8+UiBhd0pCH/arlIdiPuD\n2xHpX8RIxAq6pGCsoPJ0kwkHSw8UTnxPV8ZCPSRyHV71oQHQgSl/WjNhRi6PQroB\nSqc/pS1m09cTwyKQIopBBVayRzmI2BtBxyhQp9I8t5b7PYkEZDQlbdq0j5Xipoov\n9EW0+Zvkh1FGNig8IJ9Wp+SZi3rd7KLpkyKPY7BK/g0nXBkDxn019cET0SdJOHQG\nDiHiv4yTRsDCHZhtEbAMKZEpku4WxtQ+JjR31l8ueQKBgQDkO2oC8gi6vQDcx/CX\nZ23x2ZUyar6i0BQ8eJFAEN+IiUapEeCVazuxJSt4RjYfwSa/p117jdZGEWD0GxMC\n+iAXlc5LlrrWs4MWUc0AHTgXna28/vii3ltcsI0AjWMqaybhBTTNbMFa2/fV2OX2\nUimuFyBWbzVc3Zb9KAG4Y7OmJQKBgQC5324IjXPq5oH8UWZTdJPuO2cgRsvKmR/r\n9zl4loRjkS7FiOMfzAgUiXfH9XCnvwXMqJpuMw2PEUjUT+OyWjJONEK4qGFJkbN5\n3ykc7p5V7iPPc7Zxj4mFvJ1xjkcj+i5LY8Me+gL5mGIrJ2j8hbuv7f+PWIauyjnp\nNx/0GVFRuQKBgGNT4D1L7LSokPmFIpYh811wHliE0Fa3TDdNGZnSPhaD9/aYyy78\nLkxYKuT7WY7UVvLN+gdNoVV5NsLGDa4cAV+CWPfYr5PFKGXMT/Wewcy1WOmJ5des\nAgMC6zq0TdYmMBN6WpKUpEnQtbmh3eMnuvADLJWxbH3wCkg+4xDGg2bpAoGAYRNk\nMGtQQzqoYNNSkfus1xuHPMA8508Z8O9pwKU795R3zQs1NAInpjI1sOVrNPD7Ymwc\nW7mmNzZbxycCUL/yzg1VW4P1a6sBBYGbw1SMtWxun4ZbnuvMc2CTCh+43/1l+FHe\nMmt46kq/2rH2jwx5feTbOE6P6PINVNRJh/9BDWECgYEAsCWcH9D3cI/QDeLG1ao7\nrE2NcknP8N783edM07Z/zxWsIsXhBPY3gjHVz2LDl+QHgPWhGML62M0ja/6SsJW3\nYvLLIc82V7eqcVJTZtaFkuht68qu/Jn1ezbzJMJ4YXDYo1+KFi+2CAGR06QILb+I\nlUtj+/nH3HDQjM4ltYfTPUg=\n-----END PRIVATE KEY-----\n",
      "client_email": "foo-email@foo-project.iam.gserviceaccount.com",
      "client_id": "100000000000000000001",
      "auth_uri": "https://accounts.google.com/o/oauth2/auth",
      "token_uri": "https://oauth2.googleapis.com/token",
      "auth_provider_x509_cert_url": "https://www.googleapis.com/oauth2/v1/certs",
      "client_x509_cert_url": "https://www.googleapis.com/robot/v1/metadata/x509/foo-email%40foo-project.iam.gserviceaccount.com"
})""";

constexpr std::string_view kAuthorizedUserContents = R"""({
      "type": "authorized_user",
      "client_id": "test-client-id.apps.googleusercontent.com",
      "client_secret": "test-client-secret",
      "refresh_token": "test-refresh-token"
})""";

// The minimum gRPC needs to create file-sourced external account credentials;
// the subject token file is only read when a token is requested.
constexpr std::string_view kExternalAccountContents = R"""({
      "type": "external_account",
      "audience": "//iam.googleapis.com/projects/123456/locations/global/workloadIdentityPools/test-pool/providers/test-provider",
      "subject_token_type": "urn:ietf:params:oauth:token-type:jwt",
      "token_url": "https://sts.googleapis.com/v1/token",
      "credential_source": {"file": "/var/run/secrets/test-subject-token"}
})""";

std::string WriteTempFile(std::string_view contents) {
  std::string const filename = CreateRandomFileName();
  std::ofstream(filename) << contents;
  return filename;
}

// Tests of the library's own ADC lookup pin both of its locations, so a
// developer's real gcloud credentials can never leak into them.
// `AdcEnvironment` points `GOOGLE_APPLICATION_CREDENTIALS` at @p adc_file (or
// unsets it when empty) and the gcloud override at a file that does not exist.
// gRPC's own lookup, used when the option is off, ignores the override; the
// tests that take that path do not depend on what gRPC finds.
struct AdcEnvironment {
  explicit AdcEnvironment(std::optional<std::string> const& adc_file)
      : env("GOOGLE_APPLICATION_CREDENTIALS", adc_file),
        gcloud("GOOGLE_GCLOUD_ADC_PATH_OVERRIDE", CreateRandomFileName()) {}
  ScopedEnvironment env;
  ScopedEnvironment gcloud;
};

Options TlsOnlyOptions() {
  return Options{}.set<GrpcGoogleDefaultTlsOnlyOption>(true);
}

/// @test Verify the lookup reads the file named by the environment variable
/// and classifies a service account key.
TEST(UnifiedGrpcCredentialsTest, LookupAdcServiceAccount) {
  std::string const filename = WriteTempFile(kServiceAccountContents);
  AdcEnvironment adc(filename);
  StatusOr<AdcLookup> lookup = LookupApplicationDefaultCredentials();
  (void)std::remove(filename.c_str());
  ASSERT_STATUS_OK(lookup);
  EXPECT_EQ(lookup->source, AdcSource::kServiceAccount);
  EXPECT_EQ(lookup->path, filename);
  EXPECT_EQ(lookup->contents, kServiceAccountContents);
  EXPECT_EQ(lookup->type, "service_account");
}

/// @test Verify the lookup classifies a gcloud user credentials file.
TEST(UnifiedGrpcCredentialsTest, LookupAdcAuthorizedUser) {
  std::string const filename = WriteTempFile(kAuthorizedUserContents);
  AdcEnvironment adc(filename);
  StatusOr<AdcLookup> lookup = LookupApplicationDefaultCredentials();
  (void)std::remove(filename.c_str());
  ASSERT_STATUS_OK(lookup);
  EXPECT_EQ(lookup->source, AdcSource::kAuthorizedUser);
  EXPECT_EQ(lookup->type, "authorized_user");
}

/// @test Verify the lookup classifies an external account file.
TEST(UnifiedGrpcCredentialsTest, LookupAdcExternalAccount) {
  std::string const filename = WriteTempFile(kExternalAccountContents);
  AdcEnvironment adc(filename);
  StatusOr<AdcLookup> lookup = LookupApplicationDefaultCredentials();
  (void)std::remove(filename.c_str());
  ASSERT_STATUS_OK(lookup);
  EXPECT_EQ(lookup->source, AdcSource::kExternalAccount);
  EXPECT_EQ(lookup->type, "external_account");
}

class UnifiedGrpcCredentialsAdcTypeTest
    : public ::testing::TestWithParam<std::string> {};

/// @test Verify the `type` values that gRPC's Google Default credentials do
/// not support are reported as unsupported, including
/// `external_account_authorized_user`, which the REST credentials accept, and
/// a missing `type`.
TEST_P(UnifiedGrpcCredentialsAdcTypeTest, LookupAdcUnsupportedType) {
  std::string const& type = GetParam();
  nlohmann::json contents{{"client_id", "test-client-id"}};
  if (!type.empty()) contents["type"] = type;
  std::string const filename = WriteTempFile(contents.dump());
  AdcEnvironment adc(filename);
  StatusOr<AdcLookup> lookup = LookupApplicationDefaultCredentials();
  (void)std::remove(filename.c_str());
  ASSERT_STATUS_OK(lookup);
  EXPECT_EQ(lookup->source, AdcSource::kUnsupportedType);
  EXPECT_EQ(lookup->type, type);
}

INSTANTIATE_TEST_SUITE_P(UnifiedGrpcCredentialsTest,
                         UnifiedGrpcCredentialsAdcTypeTest,
                         ::testing::Values("external_account_authorized_user",
                                           "impersonated_service_account",
                                           "gdch_service_account",
                                           "not-a-known-type", ""),
                         [](::testing::TestParamInfo<std::string> const& info) {
                           if (info.param.empty())
                             return std::string{"MissingType"};
                           std::string name = info.param;
                           std::replace(name.begin(), name.end(), '-', '_');
                           return name;
                         });

/// @test Verify a `type` that is not a string is treated as absent rather than
/// read as a string, which would throw inside the JSON library.
TEST(UnifiedGrpcCredentialsTest, LookupAdcNonStringType) {
  std::string const filename = WriteTempFile(R"""({"type": 42})""");
  AdcEnvironment adc(filename);
  StatusOr<AdcLookup> lookup = LookupApplicationDefaultCredentials();
  (void)std::remove(filename.c_str());
  ASSERT_STATUS_OK(lookup);
  EXPECT_EQ(lookup->source, AdcSource::kUnsupportedType);
  EXPECT_THAT(lookup->type, IsEmpty());
}

struct AdcNonObjectContentsTestCase {
  std::string test_name;
  std::string contents;
};

class UnifiedGrpcCredentialsAdcContentsTest
    : public ::testing::TestWithParam<AdcNonObjectContentsTestCase> {};

/// @test Verify that files that do not hold a JSON object are reported as
/// such, rather than as an unsupported type or a crash.
TEST_P(UnifiedGrpcCredentialsAdcContentsTest, LookupAdcNotJsonObject) {
  std::string const filename = WriteTempFile(GetParam().contents);
  AdcEnvironment adc(filename);
  StatusOr<AdcLookup> lookup = LookupApplicationDefaultCredentials();
  (void)std::remove(filename.c_str());
  ASSERT_STATUS_OK(lookup);
  EXPECT_EQ(lookup->source, AdcSource::kNotJsonObject);
  EXPECT_EQ(lookup->path, filename);
}

INSTANTIATE_TEST_SUITE_P(
    UnifiedGrpcCredentialsTest, UnifiedGrpcCredentialsAdcContentsTest,
    ::testing::Values(AdcNonObjectContentsTestCase{"InvalidJson", "not json"},
                      AdcNonObjectContentsTestCase{"JsonArray", "[]"},
                      AdcNonObjectContentsTestCase{"JsonString",
                                                   R"("a string")"},
                      AdcNonObjectContentsTestCase{"JsonNumber", "42"},
                      AdcNonObjectContentsTestCase{"EmptyFile", ""}),
    [](::testing::TestParamInfo<AdcNonObjectContentsTestCase> const& info) {
      return info.param.test_name;
    });

/// @test Verify a configured file that cannot be opened is an error, as in the
/// REST credentials, rather than a silent fallback.
TEST(UnifiedGrpcCredentialsTest, LookupAdcMissingFile) {
  std::string const filename = CreateRandomFileName();
  AdcEnvironment adc(filename);
  StatusOr<AdcLookup> lookup = LookupApplicationDefaultCredentials();
  EXPECT_THAT(lookup, StatusIs(StatusCode::kUnknown,
                               HasSubstr("Cannot open credentials file")));
}

/// @test Verify the gcloud file is used when the environment variable is not
/// set.
TEST(UnifiedGrpcCredentialsTest, LookupAdcGcloudOverride) {
  std::string const filename = WriteTempFile(kServiceAccountContents);
  ScopedEnvironment env("GOOGLE_APPLICATION_CREDENTIALS", std::nullopt);
  ScopedEnvironment gcloud("GOOGLE_GCLOUD_ADC_PATH_OVERRIDE", filename);
  StatusOr<AdcLookup> lookup = LookupApplicationDefaultCredentials();
  (void)std::remove(filename.c_str());
  ASSERT_STATUS_OK(lookup);
  EXPECT_EQ(lookup->source, AdcSource::kServiceAccount);
  EXPECT_EQ(lookup->path, filename);
}

/// @test Verify a missing gcloud file is not an error: the lookup reports that
/// there is no file, which selects Compute Engine credentials.
TEST(UnifiedGrpcCredentialsTest, LookupAdcNoFile) {
  AdcEnvironment adc(std::nullopt);
  StatusOr<AdcLookup> lookup = LookupApplicationDefaultCredentials();
  ASSERT_STATUS_OK(lookup);
  EXPECT_EQ(lookup->source, AdcSource::kNone);
  EXPECT_THAT(lookup->path, IsEmpty());
  EXPECT_THAT(lookup->contents, IsEmpty());
}

/// @test Verify an empty environment variable is treated as unset, as the
/// REST credentials and gRPC do.
TEST(UnifiedGrpcCredentialsTest, LookupAdcEmptyEnvVarFallsThrough) {
  std::string const filename = WriteTempFile(kAuthorizedUserContents);
  ScopedEnvironment env("GOOGLE_APPLICATION_CREDENTIALS", "");
  ScopedEnvironment gcloud("GOOGLE_GCLOUD_ADC_PATH_OVERRIDE", filename);
  StatusOr<AdcLookup> lookup = LookupApplicationDefaultCredentials();
  (void)std::remove(filename.c_str());
  ASSERT_STATUS_OK(lookup);
  EXPECT_EQ(lookup->source, AdcSource::kAuthorizedUser);
}

/// @test Verify the lookup copes with no home directory at all. Both `HOME`
/// and `APPDATA` are unset so the test is hermetic on every platform.
TEST(UnifiedGrpcCredentialsTest, LookupAdcNoHome) {
  ScopedEnvironment env("GOOGLE_APPLICATION_CREDENTIALS", std::nullopt);
  ScopedEnvironment gcloud("GOOGLE_GCLOUD_ADC_PATH_OVERRIDE", std::nullopt);
  ScopedEnvironment home("HOME", std::nullopt);
  ScopedEnvironment appdata("APPDATA", std::nullopt);
  StatusOr<AdcLookup> lookup = LookupApplicationDefaultCredentials();
  ASSERT_STATUS_OK(lookup);
  EXPECT_EQ(lookup->source, AdcSource::kNone);
}

/// @test Verify `GrpcGoogleDefaultTlsOnlyOption` is ignored unless set: with
/// the option false the strategy is `grpc::GoogleDefaultCredentials()`, whose
/// own handling of a missing credentials file falls through to the gcloud file
/// and the metadata server instead of failing, so the strategy is created.
TEST(UnifiedGrpcCredentialsTest, WithDefaultCredentialsTlsOnlyFalseIgnoresAdc) {
  AdcEnvironment adc(CreateRandomFileName());
  CompletionQueue cq;
  std::shared_ptr<GrpcAuthenticationStrategy> auth =
      CreateAuthenticationStrategy(
          *MakeGoogleDefaultCredentials(), cq,
          Options{}.set<GrpcGoogleDefaultTlsOnlyOption>(false));
  ASSERT_THAT(auth, NotNull());
  EXPECT_FALSE(auth->RequiresConfigureContext());
  grpc::ClientContext context;
  EXPECT_THAT(auth->ConfigureContext(context), IsOk());
  EXPECT_THAT(context.credentials(), IsNull());
}

/// @test Verify a service account key yields channel-level credentials: no
/// per-call configuration, as with `grpc::GoogleDefaultCredentials()`.
TEST(UnifiedGrpcCredentialsTest, WithDefaultCredentialsTlsOnlyServiceAccount) {
  std::string const filename = WriteTempFile(kServiceAccountContents);
  AdcEnvironment adc(filename);
  CompletionQueue cq;
  std::shared_ptr<GrpcAuthenticationStrategy> auth =
      CreateAuthenticationStrategy(*MakeGoogleDefaultCredentials(), cq,
                                   TlsOnlyOptions());
  (void)std::remove(filename.c_str());
  ASSERT_THAT(auth, NotNull());
  EXPECT_FALSE(auth->RequiresConfigureContext());
  grpc::ClientContext context;
  EXPECT_THAT(auth->ConfigureContext(context), IsOk());
  EXPECT_THAT(context.credentials(), IsNull());
}

/// @test Verify gcloud user credentials yield channel-level credentials.
TEST(UnifiedGrpcCredentialsTest, WithDefaultCredentialsTlsOnlyAuthorizedUser) {
  std::string const filename = WriteTempFile(kAuthorizedUserContents);
  AdcEnvironment adc(filename);
  CompletionQueue cq;
  std::shared_ptr<GrpcAuthenticationStrategy> auth =
      CreateAuthenticationStrategy(*MakeGoogleDefaultCredentials(), cq,
                                   TlsOnlyOptions());
  (void)std::remove(filename.c_str());
  ASSERT_THAT(auth, NotNull());
  EXPECT_FALSE(auth->RequiresConfigureContext());
  grpc::ClientContext context;
  EXPECT_THAT(auth->ConfigureContext(context), IsOk());
}

/// @test Verify an external account file yields channel-level credentials.
TEST(UnifiedGrpcCredentialsTest, WithDefaultCredentialsTlsOnlyExternalAccount) {
  std::string const filename = WriteTempFile(kExternalAccountContents);
  AdcEnvironment adc(filename);
  CompletionQueue cq;
  std::shared_ptr<GrpcAuthenticationStrategy> auth =
      CreateAuthenticationStrategy(*MakeGoogleDefaultCredentials(), cq,
                                   TlsOnlyOptions());
  (void)std::remove(filename.c_str());
  ASSERT_THAT(auth, NotNull());
  EXPECT_FALSE(auth->RequiresConfigureContext());
  grpc::ClientContext context;
  EXPECT_THAT(auth->ConfigureContext(context), IsOk());
}

/// @test Verify no credentials file selects Compute Engine credentials without
/// contacting the metadata server, as the REST credentials do.
TEST(UnifiedGrpcCredentialsTest,
     WithDefaultCredentialsTlsOnlyComputeEngineFallback) {
  AdcEnvironment adc(std::nullopt);
  CompletionQueue cq;
  std::shared_ptr<GrpcAuthenticationStrategy> auth =
      CreateAuthenticationStrategy(*MakeGoogleDefaultCredentials(), cq,
                                   TlsOnlyOptions());
  ASSERT_THAT(auth, NotNull());
  EXPECT_FALSE(auth->RequiresConfigureContext());
  grpc::ClientContext context;
  EXPECT_THAT(auth->ConfigureContext(context), IsOk());
}

/// @test Verify a type that gRPC's Google Default credentials reject is
/// reported with the same message as the REST credentials.
TEST(UnifiedGrpcCredentialsTest, WithDefaultCredentialsTlsOnlyUnsupportedType) {
  std::string const filename =
      WriteTempFile(R"""({"type": "external_account_authorized_user"})""");
  AdcEnvironment adc(filename);
  CompletionQueue cq;
  std::shared_ptr<GrpcAuthenticationStrategy> auth =
      CreateAuthenticationStrategy(*MakeGoogleDefaultCredentials(), cq,
                                   TlsOnlyOptions());
  (void)std::remove(filename.c_str());
  ASSERT_THAT(auth, NotNull());
  grpc::ClientContext context;
  EXPECT_THAT(auth->ConfigureContext(context),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Unsupported credential type "
                                 "(external_account_authorized_user)")));
}

/// @test Verify a service account file with a supported `type` but no usable
/// key is an error rather than a null credentials object handed to gRPC.
TEST(UnifiedGrpcCredentialsTest,
     WithDefaultCredentialsTlsOnlyServiceAccountInvalid) {
  std::string const filename =
      WriteTempFile(R"""({"type": "service_account"})""");
  AdcEnvironment adc(filename);
  CompletionQueue cq;
  std::shared_ptr<GrpcAuthenticationStrategy> auth =
      CreateAuthenticationStrategy(*MakeGoogleDefaultCredentials(), cq,
                                   TlsOnlyOptions());
  (void)std::remove(filename.c_str());
  ASSERT_THAT(auth, NotNull());
  grpc::ClientContext context;
  EXPECT_THAT(auth->ConfigureContext(context),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("grpc::ServiceAccountJWTAccessCredentials")));
}

/// @test Verify a user credentials file without a refresh token is an error.
TEST(UnifiedGrpcCredentialsTest,
     WithDefaultCredentialsTlsOnlyAuthorizedUserInvalid) {
  std::string const filename =
      WriteTempFile(R"""({"type": "authorized_user"})""");
  AdcEnvironment adc(filename);
  CompletionQueue cq;
  std::shared_ptr<GrpcAuthenticationStrategy> auth =
      CreateAuthenticationStrategy(*MakeGoogleDefaultCredentials(), cq,
                                   TlsOnlyOptions());
  (void)std::remove(filename.c_str());
  ASSERT_THAT(auth, NotNull());
  grpc::ClientContext context;
  EXPECT_THAT(auth->ConfigureContext(context),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("grpc::GoogleRefreshTokenCredentials")));
}

/// @test Verify an external account file without a credential source is an
/// error.
TEST(UnifiedGrpcCredentialsTest,
     WithDefaultCredentialsTlsOnlyExternalAccountInvalid) {
  std::string const filename =
      WriteTempFile(R"""({"type": "external_account"})""");
  AdcEnvironment adc(filename);
  CompletionQueue cq;
  std::shared_ptr<GrpcAuthenticationStrategy> auth =
      CreateAuthenticationStrategy(*MakeGoogleDefaultCredentials(), cq,
                                   TlsOnlyOptions());
  (void)std::remove(filename.c_str());
  ASSERT_THAT(auth, NotNull());
  grpc::ClientContext context;
  EXPECT_THAT(auth->ConfigureContext(context),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("grpc::ExternalAccountCredentials")));
}

/// @test Verify a file that is not a JSON object is reported as such.
TEST(UnifiedGrpcCredentialsTest, WithDefaultCredentialsTlsOnlyNotJsonObject) {
  std::string const filename = WriteTempFile("not json");
  AdcEnvironment adc(filename);
  CompletionQueue cq;
  std::shared_ptr<GrpcAuthenticationStrategy> auth =
      CreateAuthenticationStrategy(*MakeGoogleDefaultCredentials(), cq,
                                   TlsOnlyOptions());
  (void)std::remove(filename.c_str());
  ASSERT_THAT(auth, NotNull());
  grpc::ClientContext context;
  EXPECT_THAT(auth->ConfigureContext(context),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("does not contain a JSON object")));
}

/// @test Verify a configured file that cannot be opened fails every RPC with
/// the same status as the REST credentials.
TEST(UnifiedGrpcCredentialsTest, WithDefaultCredentialsTlsOnlyMissingFile) {
  AdcEnvironment adc(CreateRandomFileName());
  CompletionQueue cq;
  std::shared_ptr<GrpcAuthenticationStrategy> auth =
      CreateAuthenticationStrategy(*MakeGoogleDefaultCredentials(), cq,
                                   TlsOnlyOptions());
  ASSERT_THAT(auth, NotNull());
  grpc::ClientContext context;
  EXPECT_THAT(auth->ConfigureContext(context),
              StatusIs(StatusCode::kUnknown,
                       HasSubstr("Cannot open credentials file")));
}

/// @test Verify the strategy is created when `CARootsFilePathOption` names a
/// readable file. The roots themselves are not observable through
/// `grpc::ChannelCredentials`; `LoadCAInfo()` is tested above.
TEST(UnifiedGrpcCredentialsTest,
     WithDefaultCredentialsTlsOnlyCustomCACertPath) {
  std::string const ca_filename = WriteTempFile(kCACertificate);
  std::string const filename = WriteTempFile(kServiceAccountContents);
  AdcEnvironment adc(filename);
  CompletionQueue cq;
  std::shared_ptr<GrpcAuthenticationStrategy> auth =
      CreateAuthenticationStrategy(
          *MakeGoogleDefaultCredentials(), cq,
          TlsOnlyOptions().set<CARootsFilePathOption>(ca_filename));
  (void)std::remove(ca_filename.c_str());
  (void)std::remove(filename.c_str());
  ASSERT_THAT(auth, NotNull());
  EXPECT_FALSE(auth->RequiresConfigureContext());
  grpc::ClientContext context;
  EXPECT_THAT(auth->ConfigureContext(context), IsOk());
}

/// @test Verify the strategy is created when `CARootsFilePathOption` names a
/// missing file, as it is for the other TLS strategies (`LoadCAInfo()` yields
/// empty roots).
TEST(UnifiedGrpcCredentialsTest,
     WithDefaultCredentialsTlsOnlyMissingCACertFile) {
  std::string const filename = WriteTempFile(kServiceAccountContents);
  AdcEnvironment adc(filename);
  CompletionQueue cq;
  std::shared_ptr<GrpcAuthenticationStrategy> auth =
      CreateAuthenticationStrategy(
          *MakeGoogleDefaultCredentials(), cq,
          TlsOnlyOptions().set<CARootsFilePathOption>(CreateRandomFileName()));
  (void)std::remove(filename.c_str());
  ASSERT_THAT(auth, NotNull());
  EXPECT_FALSE(auth->RequiresConfigureContext());
  grpc::ClientContext context;
  EXPECT_THAT(auth->ConfigureContext(context), IsOk());
}

/// @test Verify the option only affects Google Default credentials: an
/// explicit credential type keeps its per-call strategy.
TEST(UnifiedGrpcCredentialsTest,
     WithAccessTokenCredentialsIgnoresTlsOnlyOption) {
  CompletionQueue cq;
  std::shared_ptr<GrpcAuthenticationStrategy> auth =
      CreateAuthenticationStrategy(
          *MakeAccessTokenCredentials(
              "test-token",
              std::chrono::system_clock::now() + std::chrono::minutes(15)),
          cq, TlsOnlyOptions());
  ASSERT_THAT(auth, NotNull());
  EXPECT_TRUE(auth->RequiresConfigureContext());
  grpc::ClientContext context;
  EXPECT_THAT(auth->ConfigureContext(context), IsOk());
  EXPECT_THAT(context.credentials(), NotNull());
}

/// @test Verify the option reaches the base credentials of impersonated
/// credentials, which are built with the same options for the IAM channel: a
/// missing credentials file surfaces as the TLS strategy's error instead of
/// being handled by `grpc::GoogleDefaultCredentials()`. The error is produced
/// before any RPC is attempted; the completion queue only delivers it, so the
/// test needs no network.
TEST(UnifiedGrpcCredentialsTest,
     WithImpersonateServiceAccountCredentialsTlsOnlyBaseCredentials) {
  AdcEnvironment adc(CreateRandomFileName());
  AutomaticallyCreatedBackgroundThreads background;
  std::shared_ptr<GrpcAuthenticationStrategy> auth =
      CreateAuthenticationStrategy(
          *MakeImpersonateServiceAccountCredentials(
              MakeGoogleDefaultCredentials(),
              "test-only@test-project.iam.gserviceaccount.com"),
          background.cq(), TlsOnlyOptions());
  ASSERT_THAT(auth, NotNull());
  EXPECT_TRUE(auth->RequiresConfigureContext());
  grpc::ClientContext context;
  EXPECT_THAT(auth->ConfigureContext(context),
              StatusIs(StatusCode::kUnknown,
                       HasSubstr("Cannot open credentials file")));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
