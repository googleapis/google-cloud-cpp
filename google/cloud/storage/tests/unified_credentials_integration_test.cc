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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/unified_rest_credentials.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/storage/testing/temp_file.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#ifndef _WIN32
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif
#include <fstream>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::MakeAccessTokenCredentials;
using ::google::cloud::MakeGoogleDefaultCredentials;
using ::google::cloud::MakeServiceAccountCredentials;
using ::google::cloud::UnifiedCredentialsOption;
using ::google::cloud::internal::GetEnv;
using ::google::cloud::storage::testing::TempFile;
using ::google::cloud::testing_util::IsOk;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::StartsWith;

// This is a properly formatted, but invalid, CA Certificate. We will use this
// as the *only* root of trust and try to contact *.google.com. This will
// (naturally) fail, which is the expected result. A separate test will verify
// that using a *valid* CA certificate (bundle) works.
//
// Created with:
//
// openssl genrsa -des3 -out cert.key 2048
// openssl req -x509 -new -nodes -key cert.key
//     -sha256 -days 3650 -out cert.pem
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

constexpr int kCurleAbortedByCallback = 42;
// constexpr int kCurleOk = 0;

class UnifiedCredentialsIntegrationTest
    : public ::google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  UnifiedCredentialsIntegrationTest() = default;

  void SetUp() override {
    bucket_name_ =
        GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME").value_or("");
    project_id_ = GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
    service_account_ =
        GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_SERVICE_ACCOUNT")
            .value_or("");
    roots_pem_ = GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_ROOTS_PEM").value_or("");

    ASSERT_THAT(bucket_name_, Not(IsEmpty()));
    ASSERT_THAT(project_id_, Not(IsEmpty()));
    ASSERT_THAT(service_account_, Not(IsEmpty()));
    ASSERT_THAT(roots_pem_, Not(IsEmpty()));

    std::ifstream is(roots_pem_);
    ca_certs_text_ = std::string{std::istreambuf_iterator<char>{is}, {}};
    std::string begin_cert = "-----BEGIN CERTIFICATE-----";
    std::string end_cert = "-----END CERTIFICATE-----";
    size_t current = 0;
    size_t begin;
    while ((begin = ca_certs_text_.find(begin_cert, current)) !=
           std::string::npos) {
      auto end = ca_certs_text_.find(end_cert, begin);
      current = end + end_cert.length();
      ca_certs_.emplace_back(&ca_certs_text_[begin], current - begin);
    }
    ASSERT_THAT(ca_certs_, Not(IsEmpty()));
  }

  static Client MakeTestClient(Options opts) {
    return MakeIntegrationTestClient(std::move(opts));
  }

  std::string const& bucket_name() const { return bucket_name_; }
  std::string const& project_id() const { return project_id_; }
  std::string const& service_account() const { return service_account_; }
  std::string roots_pem() const { return roots_pem_; }
  std::string invalid_pem() const { return invalid_pem_.name(); }
  std::string empty_file() const { return empty_file_.name(); }

  static Options TestOptions() {
    return Options{}.set<RetryPolicyOption>(
        LimitedErrorCountRetryPolicy(3).clone());
  }

  Options EmptyTrustStoreOptions() {
    return TestOptions()
        // Use the trust store with no valid just an invalid CA certificate.
        .set<CARootsFilePathOption>(invalid_pem())
        // Disable the default CAPath in libcurl, no effect on gRPC.
        .set<internal::CAPathOption>(empty_file());
  }

  Options CustomTrustStoreOptions() {
    return TestOptions()
        // Use the custom trust store with Google's root CA certificates.
        .set<CARootsFilePathOption>(roots_pem())
        // Disable the default CAPath in libcurl, no effect on gRPC.
        .set<internal::CAPathOption>(empty_file());
  }

  Options ValidInMemoryTrustStoreOptions() {
    return TestOptions()
        // Populates in-memory trust store with Google's root CA certificates.
        .set<experimental::CAInMemoryOption>(ca_certs_);
  }

  static Options InvalidInMemoryTrustStoreOptions() {
    std::vector<absl::string_view> bogus_certs;
    bogus_certs.emplace_back(kCACertificate);

    return TestOptions()
        // Populates in-memory trust store with an invalid CA certificate.
        .set<experimental::CAInMemoryOption>(bogus_certs);
  }

  //  Options ValidCAStoreFromSslCtxCallbackOptions() {
  //    auto ssl_ctx_callback = [ca_certs = ca_certs_](void*, void* ssl_ctx,
  //                                                   void*) -> int {
  // #ifndef _WIN32
  //      struct BIOPtrCleanup {
  //        int operator()(BIO* b) const { return BIO_free(b); }
  //      };
  //      using BioPtr = std::unique_ptr<BIO, BIOPtrCleanup>;
  //      struct X509InfoPtrCleanup {
  //        void operator()(STACK_OF(X509_INFO) * i) const {
  //          return sk_X509_INFO_pop_free(i, X509_INFO_free);
  //        }
  //      };
  //      using X509InfoPtr =
  //          std::unique_ptr<STACK_OF(X509_INFO), X509InfoPtrCleanup>;
  //
  //      X509_STORE* cert_store =
  //          SSL_CTX_get_cert_store(static_cast<SSL_CTX*>(ssl_ctx));
  //      if (!cert_store) {
  //        return kCurleAbortedByCallback;
  //      }
  //
  //      // Add each of the provided certs to the store.
  //      for (auto const& cert : ca_certs) {
  //        BioPtr buf{
  //            BIO_new_mem_buf(cert.data(), static_cast<int>(cert.length()))};
  //        if (!buf) {
  //          return kCurleAbortedByCallback;
  //        }
  //        X509InfoPtr info{
  //            PEM_X509_INFO_read_bio(buf.get(), nullptr, nullptr, nullptr)};
  //        if (!info) {
  //          return kCurleAbortedByCallback;
  //        }
  //
  //        for (decltype(sk_X509_INFO_num(info.get())) i = 0;
  //             i < sk_X509_INFO_num(info.get()); ++i) {
  //          X509_INFO* value = sk_X509_INFO_value(info.get(), i);
  //          if (value->x509) {
  //            X509_STORE_add_cert(cert_store, value->x509);
  //          }
  //          if (value->crl) {
  //            X509_STORE_add_crl(cert_store, value->crl);
  //          }
  //        }
  //      }
  //      return kCurleOk;
  // #else
  //      return kCurleAbortedByCallback;
  // #endif
  //    };
  //
  //    return TestOptions()
  //        // Populates in-memory trust store with Google's root CA
  //        certificates.
  //        .set<experimental::SslCtxCallbackOption>(ssl_ctx_callback);
  //  }

  static Options InvalidCAStoreFromSslCtxCallbackOptions() {
    auto ssl_ctx_callback = [=](void*, void*, void*) -> int {
      return kCurleAbortedByCallback;
    };
    return TestOptions()
        // Populates in-memory trust store with an invalid CA certificate.
        .set<experimental::SslCtxCallbackOption>(ssl_ctx_callback);
  }

  void UseClient(Client client, std::string const& bucket_name,
                 std::string const& object_name, std::string const& payload) {
    StatusOr<ObjectMetadata> meta = client.InsertObject(
        bucket_name, object_name, payload, IfGenerationMatch(0));
    ASSERT_THAT(meta, IsOk());
    ScheduleForDelete(*meta);

    auto stream = client.ReadObject(bucket_name, object_name);
    std::string actual(std::istreambuf_iterator<char>{stream}, {});
    EXPECT_EQ(payload, actual);
  }

  void ExpectInsertFailure(Client client, std::string const& bucket_name,
                           std::string const& object_name) {
    auto meta = client.InsertObject(bucket_name, object_name, LoremIpsum(),
                                    IfGenerationMatch(0));
    EXPECT_THAT(meta, Not(IsOk()));
    if (meta) ScheduleForDelete(*meta);
  }

 private:
  std::string bucket_name_;
  std::string project_id_;
  std::string service_account_;
  std::string roots_pem_;
  std::string ca_certs_text_;
  std::vector<absl::string_view> ca_certs_;
  TempFile invalid_pem_{kCACertificate};
  TempFile empty_file_{std::string{}};
};

TEST_F(UnifiedCredentialsIntegrationTest, GoogleDefaultCredentials) {
  if (UsingEmulator()) GTEST_SKIP();
  auto client = MakeTestClient(
      Options{}.set<UnifiedCredentialsOption>(MakeGoogleDefaultCredentials()));

  ASSERT_NO_FATAL_FAILURE(
      UseClient(client, bucket_name(), MakeRandomObjectName(), LoremIpsum()));
}

TEST_F(UnifiedCredentialsIntegrationTest, SAImpersonation) {
  if (UsingEmulator()) GTEST_SKIP();

  auto client = MakeTestClient(Options{}.set<UnifiedCredentialsOption>(
      MakeImpersonateServiceAccountCredentials(MakeGoogleDefaultCredentials(),
                                               service_account())));

  ASSERT_NO_FATAL_FAILURE(
      UseClient(client, bucket_name(), MakeRandomObjectName(), LoremIpsum()));
}

TEST_F(UnifiedCredentialsIntegrationTest, SAImpersonationCustomTrustStore) {
  if (UsingEmulator()) GTEST_SKIP();

  auto client =
      MakeTestClient(CustomTrustStoreOptions().set<UnifiedCredentialsOption>(
          MakeImpersonateServiceAccountCredentials(
              MakeGoogleDefaultCredentials(), service_account(),
              CustomTrustStoreOptions())));

  ASSERT_NO_FATAL_FAILURE(
      UseClient(client, bucket_name(), MakeRandomObjectName(), LoremIpsum()));
}

TEST_F(UnifiedCredentialsIntegrationTest, SAImpersonationEmptyTrustStore) {
  if (UsingEmulator()) GTEST_SKIP();

  auto client =
      MakeTestClient(EmptyTrustStoreOptions().set<UnifiedCredentialsOption>(
          MakeImpersonateServiceAccountCredentials(
              MakeGoogleDefaultCredentials(), service_account(),
              EmptyTrustStoreOptions())));

  ASSERT_NO_FATAL_FAILURE(
      ExpectInsertFailure(client, bucket_name(), MakeRandomObjectName()));
}

TEST_F(UnifiedCredentialsIntegrationTest, ServiceAccount) {
  if (UsingEmulator()) GTEST_SKIP();
  auto keyfile = GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_JSON");
  if (!keyfile.has_value()) GTEST_SKIP();

  auto contents = [](std::string const& filename) {
    std::ifstream is(filename);
    return std::string{std::istreambuf_iterator<char>{is}, {}};
  }(keyfile.value());

  auto client = MakeTestClient(Options{}.set<UnifiedCredentialsOption>(
      MakeServiceAccountCredentials(contents)));

  ASSERT_NO_FATAL_FAILURE(
      UseClient(client, bucket_name(), MakeRandomObjectName(), LoremIpsum()));
}

TEST_F(UnifiedCredentialsIntegrationTest, ServiceAccountCustomTrustStore) {
  if (UsingEmulator()) GTEST_SKIP();
  auto keyfile = GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_JSON");
  if (!keyfile.has_value()) GTEST_SKIP();

  auto contents = [](std::string const& filename) {
    std::ifstream is(filename);
    return std::string{std::istreambuf_iterator<char>{is}, {}};
  }(keyfile.value());

  auto client =
      MakeTestClient(CustomTrustStoreOptions().set<UnifiedCredentialsOption>(
          MakeServiceAccountCredentials(contents, CustomTrustStoreOptions())));

  ASSERT_NO_FATAL_FAILURE(
      UseClient(client, bucket_name(), MakeRandomObjectName(), LoremIpsum()));
}

TEST_F(UnifiedCredentialsIntegrationTest, ServiceAccountEmptyTrustStore) {
  if (UsingEmulator()) GTEST_SKIP();
  auto keyfile = GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_JSON");
  if (!keyfile.has_value()) GTEST_SKIP();

  auto contents = [](std::string const& filename) {
    std::ifstream is(filename);
    return std::string{std::istreambuf_iterator<char>{is}, {}};
  }(keyfile.value());

  auto client =
      MakeTestClient(EmptyTrustStoreOptions().set<UnifiedCredentialsOption>(
          MakeServiceAccountCredentials(contents, EmptyTrustStoreOptions())));

  ASSERT_NO_FATAL_FAILURE(
      ExpectInsertFailure(client, bucket_name(), MakeRandomObjectName()));
}

TEST_F(UnifiedCredentialsIntegrationTest, AccessToken) {
  if (UsingEmulator()) GTEST_SKIP();
  // First use the default credentials to obtain an access token, then use the
  // access token to test the AccessTokenCredentials() function. In a real
  // application one would fetch access tokens from something more interesting,
  // like the IAM credentials service. This is just a reasonably easy way to get
  // a working access token for the test.
  auto default_credentials = oauth2::GoogleDefaultCredentials();
  ASSERT_THAT(default_credentials, IsOk());
  auto expiration = std::chrono::system_clock::now() + std::chrono::hours(1);
  auto header = default_credentials.value()->AuthorizationHeader();
  ASSERT_THAT(header, IsOk());

  auto constexpr kPrefix = "Authorization: Bearer ";
  ASSERT_THAT(*header, StartsWith(kPrefix));
  auto token = header->substr(std::strlen(kPrefix));

  auto client = MakeTestClient(Options{}.set<UnifiedCredentialsOption>(
      MakeAccessTokenCredentials(token, expiration)));

  ASSERT_NO_FATAL_FAILURE(
      UseClient(client, bucket_name(), MakeRandomObjectName(), LoremIpsum()));
}

TEST_F(UnifiedCredentialsIntegrationTest, AccessTokenCustomTrustStore) {
  if (UsingEmulator()) GTEST_SKIP();

  // First use the default credentials to obtain an access token, then use the
  // access token to test the AccessTokenCredentials() function. In a real
  // application one would fetch access tokens from something more interesting,
  // like the IAM credentials service. This is just a reasonably easy way to get
  // a working access token for the test.
  auto default_credentials = oauth2::GoogleDefaultCredentials();
  ASSERT_THAT(default_credentials, IsOk());
  auto expiration = std::chrono::system_clock::now() + std::chrono::hours(1);
  auto header = default_credentials.value()->AuthorizationHeader();
  ASSERT_THAT(header, IsOk());

  auto constexpr kPrefix = "Authorization: Bearer ";
  ASSERT_THAT(*header, StartsWith(kPrefix));
  auto token = header->substr(std::strlen(kPrefix));

  testing_util::ScopedEnvironment grpc_roots_pem(
      "GRPC_DEFAULT_SSL_ROOTS_FILE_PATH", absl::nullopt);

  auto client =
      MakeTestClient(CustomTrustStoreOptions().set<UnifiedCredentialsOption>(
          MakeAccessTokenCredentials(token, expiration)));

  ASSERT_NO_FATAL_FAILURE(
      UseClient(client, bucket_name(), MakeRandomObjectName(), LoremIpsum()));
}

TEST_F(UnifiedCredentialsIntegrationTest, AccessTokenEmptyTrustStore) {
  if (UsingEmulator()) GTEST_SKIP();
  // First use the default credentials to obtain an access token, then use the
  // access token to test the AccessTokenCredentials() function. In a real
  // application one would fetch access tokens from something more interesting,
  // like the IAM credentials service. This is just a reasonably easy way to get
  // a working access token for the test.
  auto default_credentials = oauth2::GoogleDefaultCredentials();
  ASSERT_THAT(default_credentials, IsOk());
  auto expiration = std::chrono::system_clock::now() + std::chrono::hours(1);
  auto header = default_credentials.value()->AuthorizationHeader();
  ASSERT_THAT(header, IsOk());

  auto constexpr kPrefix = "Authorization: Bearer ";
  ASSERT_THAT(*header, StartsWith(kPrefix));
  auto token = header->substr(std::strlen(kPrefix));

  auto client = MakeTestClient(
      EmptyTrustStoreOptions()
          .set<UnifiedCredentialsOption>(
              MakeAccessTokenCredentials(token, expiration))
          .set<RetryPolicyOption>(LimitedErrorCountRetryPolicy(2).clone()));

  EXPECT_NO_FATAL_FAILURE(
      ExpectInsertFailure(client, bucket_name(), MakeRandomObjectName()));
}

TEST_F(UnifiedCredentialsIntegrationTest, ValidCAStoreInMemory) {
  if (GetEnv("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG") != "none") GTEST_SKIP();
  if (UsingEmulator()) GTEST_SKIP();
  auto keyfile = GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_JSON");
  if (!keyfile.has_value()) GTEST_SKIP();

  auto contents = [](std::string const& filename) {
    std::ifstream is(filename);
    return std::string{std::istreambuf_iterator<char>{is}, {}};
  }(keyfile.value());

  auto client = MakeTestClient(
      ValidInMemoryTrustStoreOptions().set<UnifiedCredentialsOption>(
          MakeServiceAccountCredentials(contents)));

  ASSERT_NO_FATAL_FAILURE(
      UseClient(client, bucket_name(), MakeRandomObjectName(), LoremIpsum()));
}

TEST_F(UnifiedCredentialsIntegrationTest, InvalidCAStoreInMemory) {
  if (GetEnv("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG") != "none") GTEST_SKIP();
  if (UsingEmulator()) GTEST_SKIP();
  auto keyfile = GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_JSON");
  if (!keyfile.has_value()) GTEST_SKIP();

  auto contents = [](std::string const& filename) {
    std::ifstream is(filename);
    return std::string{std::istreambuf_iterator<char>{is}, {}};
  }(keyfile.value());

  auto client = MakeTestClient(
      InvalidInMemoryTrustStoreOptions().set<UnifiedCredentialsOption>(
          MakeServiceAccountCredentials(contents)));

  EXPECT_NO_FATAL_FAILURE(
      ExpectInsertFailure(client, bucket_name(), MakeRandomObjectName()));
}

#ifndef _WIN32
// TEST_F(UnifiedCredentialsIntegrationTest, ValidCAStoreFromSslCtxCallback) {
//   if (GetEnv("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG") != "none") GTEST_SKIP();
//   if (UsingEmulator()) GTEST_SKIP();
//   auto keyfile = GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_JSON");
//   if (!keyfile.has_value()) GTEST_SKIP();
//
//   auto contents = [](std::string const& filename) {
//     std::ifstream is(filename);
//     return std::string{std::istreambuf_iterator<char>{is}, {}};
//   }(keyfile.value());
//
//   auto client = MakeTestClient(
//       ValidCAStoreFromSslCtxCallbackOptions().set<UnifiedCredentialsOption>(
//           MakeServiceAccountCredentials(contents)));
//
//   ASSERT_NO_FATAL_FAILURE(
//       UseClient(client, bucket_name(), MakeRandomObjectName(),
//       LoremIpsum()));
// }

TEST_F(UnifiedCredentialsIntegrationTest, InvalidCAStoreFromSslCtxCallback) {
  if (GetEnv("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG") != "none") GTEST_SKIP();
  if (UsingEmulator()) GTEST_SKIP();
  auto keyfile = GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_JSON");
  if (!keyfile.has_value()) GTEST_SKIP();

  auto contents = [](std::string const& filename) {
    std::ifstream is(filename);
    return std::string{std::istreambuf_iterator<char>{is}, {}};
  }(keyfile.value());

  auto client = MakeTestClient(
      InvalidCAStoreFromSslCtxCallbackOptions().set<UnifiedCredentialsOption>(
          MakeServiceAccountCredentials(contents)));

  EXPECT_NO_FATAL_FAILURE(
      ExpectInsertFailure(client, bucket_name(), MakeRandomObjectName()));
}
#endif

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
