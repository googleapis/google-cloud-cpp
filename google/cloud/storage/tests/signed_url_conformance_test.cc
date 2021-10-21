// Copyright 2019 Google LLC
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

#include "google/cloud/storage/testing/storage_integration_test.h"
#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/storage/internal/signed_url_requests.h"
#include "google/cloud/storage/list_objects_reader.h"
#include "google/cloud/storage/tests/conformance_tests.pb.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/time_utils.h"
#include "google/cloud/terminate_handler.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <google/protobuf/util/json_util.h>
#include <gmock/gmock.h>
#include <fstream>
#include <type_traits>

/**
 * @file
 *
 * Executes V4 signed URLs conformance tests described in an external file.
 *
 * We have a common set of conformance tests for V4 signed URLs used in all the
 * GCS client libraries. The tests are stored in an external JSON file. This
 * program receives the file name as an input parameter, loads it, and executes
 * the tests described in the file.
 *
 * A separate command-line argument is the name of a (invalidated) service
 * account key file used to create the signed URLs.
 */

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
using google::cloud::conformance::storage::v1::PostPolicyV4Test;
using google::cloud::conformance::storage::v1::SigningV4Test;
using google::cloud::conformance::storage::v1::UrlStyle;
using ::testing::HasSubstr;

// Initialized in main() below.
std::map<std::string, SigningV4Test>* signing_tests;
std::map<std::string, PostPolicyV4Test>* post_policy_tests;

class V4SignedUrlConformanceTest
    : public google::cloud::storage::testing::StorageIntegrationTest,
      public ::testing::WithParamInterface<std::string> {
 protected:
  void SetUp() override {
    service_account_key_filename_ =
        google::cloud::internal::GetEnv(
            "GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_KEYFILE")
            .value_or("");
    ASSERT_FALSE(service_account_key_filename_.empty());
  }

  std::string service_account_key_filename_;
};

class V4PostPolicyConformanceTest : public V4SignedUrlConformanceTest {};

TEST_P(V4SignedUrlConformanceTest, V4SignJson) {
  auto creds = oauth2::CreateServiceAccountCredentialsFromJsonFilePath(
      service_account_key_filename_);
  ASSERT_STATUS_OK(creds);

  std::string account_email = (*creds)->AccountEmail();
  Client client(Options{}.set<Oauth2CredentialsOption>(*creds));
  std::string actual_canonical_request;
  std::string actual_string_to_sign;

  auto const& test_params = (*signing_tests)[GetParam()];
  auto const& method_name = test_params.method();
  auto const& bucket_name = test_params.bucket();
  auto const& object_name = test_params.object();
  auto const& scheme = test_params.scheme();
  auto const& url_style = test_params.urlstyle();
  auto const date =
      ::google::cloud::internal::ToChronoTimePoint(test_params.timestamp());
  auto const valid_for = std::chrono::seconds(test_params.expiration());
  auto const& expected = test_params.expectedurl();
  auto const& expected_canonical_request =
      test_params.expectedcanonicalrequest();
  auto const& expected_string_to_sign = test_params.expectedstringtosign();

  // Extract the headers for each object
  auto headers = test_params.headers();
  auto params = test_params.query_parameters();

  google::cloud::storage::internal::V4SignUrlRequest request(
      method_name, bucket_name, object_name);
  request.set_multiple_options(SignedUrlTimestamp(date),
                               SignedUrlDuration(valid_for));

  std::vector<AddExtensionHeaderOption> header_extensions(5);
  ASSERT_LE(headers.size(), header_extensions.size());
  std::size_t idx = 0;
  for (auto const& name_val : headers) {
    request.set_multiple_options(
        AddExtensionHeader(name_val.first, name_val.second));
    header_extensions[idx++] =
        AddExtensionHeader(name_val.first, name_val.second);
  }

  std::vector<AddQueryParameterOption> query_params(5);
  ASSERT_LE(params.size(), query_params.size());
  idx = 0;
  for (auto const& name_val : params) {
    request.set_multiple_options(
        AddQueryParameterOption(name_val.first, name_val.second));
    query_params[idx++] =
        AddQueryParameterOption(name_val.first, name_val.second);
  }

  VirtualHostname virtual_hotname;
  if (url_style == UrlStyle::VIRTUAL_HOSTED_STYLE) {
    virtual_hotname = VirtualHostname(true);
    request.set_multiple_options(VirtualHostname(true));
  }

  BucketBoundHostname domain_named_bucket;
  if (url_style == UrlStyle::BUCKET_BOUND_HOSTNAME) {
    domain_named_bucket =
        BucketBoundHostname(test_params.bucketboundhostname());
    request.set_multiple_options(
        BucketBoundHostname(test_params.bucketboundhostname()));
  }

  auto actual = client.CreateV4SignedUrl(
      method_name, bucket_name, object_name, SignedUrlTimestamp(date),
      SignedUrlDuration(valid_for), header_extensions[0], header_extensions[1],
      header_extensions[2], header_extensions[3], header_extensions[4],
      query_params[0], query_params[1], query_params[2], query_params[3],
      query_params[4], virtual_hotname, domain_named_bucket, Scheme(scheme));
  ASSERT_STATUS_OK(request.Validate());
  request.AddMissingRequiredHeaders();
  ASSERT_STATUS_OK(request.Validate());

  actual_string_to_sign = request.StringToSign(account_email);
  actual_canonical_request = request.CanonicalRequest(account_email);

  ASSERT_STATUS_OK(actual);
  if (!domain_named_bucket.has_value()) {
    EXPECT_THAT(*actual, HasSubstr(bucket_name));
  }
  EXPECT_EQ(expected, *actual);
  EXPECT_EQ(expected_canonical_request, actual_canonical_request);
  EXPECT_EQ(expected_string_to_sign, actual_string_to_sign);
}

INSTANTIATE_TEST_SUITE_P(
    V4SignedUrlConformanceTest, V4SignedUrlConformanceTest,
    ::testing::ValuesIn([] {
      std::vector<std::string> res;
      std::transform(signing_tests->begin(), signing_tests->end(),
                     std::back_inserter(res),
                     [](std::pair<std::string, SigningV4Test> const& p) {
                       return p.first;
                     });
      return res;
    }()));

TEST_P(V4PostPolicyConformanceTest, V4PostPolicy) {
  auto creds = oauth2::CreateServiceAccountCredentialsFromJsonFilePath(
      service_account_key_filename_);
  ASSERT_STATUS_OK(creds);

  std::string account_email = (*creds)->AccountEmail();
  Client client(Options{}.set<Oauth2CredentialsOption>(*creds));

  auto const& test_params = (*post_policy_tests)[GetParam()];
  auto const& input = test_params.policyinput();
  auto const& output = test_params.policyoutput();
  auto const& bucket_name = input.bucket();
  auto const& object_name = input.object();
  auto const valid_for = std::chrono::seconds(input.expiration());
  auto const timestamp =
      ::google::cloud::internal::ToChronoTimePoint(input.timestamp());
  auto const& scheme = input.scheme();
  BucketBoundHostname domain_named_bucket;
  auto const& url_style = input.urlstyle();
  if (url_style == UrlStyle::BUCKET_BOUND_HOSTNAME) {
    domain_named_bucket = BucketBoundHostname(input.bucketboundhostname());
  }
  VirtualHostname virtual_hotname;
  if (url_style == UrlStyle::VIRTUAL_HOSTED_STYLE) {
    virtual_hotname = VirtualHostname(true);
  }

  std::vector<PolicyDocumentCondition> conditions;
  auto const& condition = input.conditions();

  ASSERT_TRUE(condition.startswith().empty() ||
              condition.startswith().size() == 2U)
      << condition.startswith().size();
  if (condition.startswith().size() == 2U) {
    conditions.emplace_back(PolicyDocumentCondition::StartsWith(
        condition.startswith()[0].substr(1), condition.startswith()[1]));
  }
  ASSERT_TRUE(condition.contentlengthrange().empty() ||
              condition.contentlengthrange().size() == 2U)
      << condition.contentlengthrange().size();
  if (condition.contentlengthrange().size() == 2U) {
    conditions.emplace_back(PolicyDocumentCondition::ContentLengthRange(
        condition.contentlengthrange()[0], condition.contentlengthrange()[1]));
  }

  auto const& expected_url = output.url();
  auto const& fields = output.fields();
  auto const& expected_algorithm = fields.at("x-goog-algorithm");
  auto const& expected_credential = fields.at("x-goog-credential");
  auto const& expected_date = fields.at("x-goog-date");
  auto const& expected_signature = fields.at("x-goog-signature");
  auto const& expected_policy = fields.at("policy");

  // We need to escape it because `nlohmann::json` interprets the escaped
  // characters.
  std::string const expected_decoded_policy =
      *internal::PostPolicyV4Escape(output.expecteddecodedpolicy());

  std::vector<AddExtensionFieldOption> extension_fields(5);
  ASSERT_LE(input.fields().size(), extension_fields.size());
  std::size_t idx = 0;
  for (auto const& name_val : input.fields()) {
    extension_fields[idx++] =
        AddExtensionField(name_val.first, name_val.second);
  }
  PolicyDocumentV4 doc{bucket_name, object_name, valid_for, timestamp,
                       std::move(conditions)};
  auto doc_res = client.GenerateSignedPostPolicyV4(
      doc, extension_fields[0], extension_fields[1], extension_fields[2],
      extension_fields[3], extension_fields[4], Scheme(scheme),
      domain_named_bucket, virtual_hotname);
  ASSERT_STATUS_OK(doc_res);
  EXPECT_EQ(expected_policy, doc_res->policy);
  auto actual_policy_vec = internal::Base64Decode(doc_res->policy);
  std::string actual_policy(actual_policy_vec.begin(), actual_policy_vec.end());
  EXPECT_EQ(expected_decoded_policy, actual_policy);
  EXPECT_EQ(expected_url, doc_res->url);
  EXPECT_EQ(expected_credential, doc_res->access_id);
  EXPECT_EQ(expected_date, google::cloud::internal::FormatV4SignedUrlTimestamp(
                               doc_res->expiration - valid_for));
  EXPECT_EQ(expected_algorithm, doc_res->signing_algorithm);
  EXPECT_EQ(expected_signature, doc_res->signature);
  EXPECT_EQ((std::map<std::string, std::string>(fields.begin(), fields.end())),
            doc_res->required_form_fields);
}

INSTANTIATE_TEST_SUITE_P(
    V4PostPolicyConformanceTest, V4PostPolicyConformanceTest,
    ::testing::ValuesIn([] {
      std::vector<std::string> res;
      std::transform(post_policy_tests->begin(), post_policy_tests->end(),
                     std::back_inserter(res),
                     [](std::pair<std::string, PostPolicyV4Test> const& p) {
                       return p.first;
                     });
      return res;
    }()));

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

using google::cloud::conformance::storage::v1::PostPolicyV4Test;
using google::cloud::conformance::storage::v1::SigningV4Test;

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  auto conformance_tests_file =
      google::cloud::internal::GetEnv(
          "GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_CONFORMANCE_FILENAME")
          .value_or("");
  if (conformance_tests_file.empty()) {
    std::cerr
        << "The GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_CONFORMANCE_FILENAME"
        << " environment variable must be set and not empty.\n";
    return 1;
  }

  std::ifstream ifstr(conformance_tests_file);
  if (!ifstr.is_open()) {
    std::cerr << "Failed to open data file: \"" << conformance_tests_file
              << "\"\n";
    return 1;
  }
  std::string json_rep(std::istreambuf_iterator<char>{ifstr}, {});
  google::cloud::conformance::storage::v1::TestFile tests;
  auto status = google::protobuf::util::JsonStringToMessage(json_rep, &tests);
  if (!status.ok()) {
    std::cerr << "Failed to parse conformance tests: " << status.ToString()
              << "\n";
    return 1;
  }

  auto signing_tests_destroyer =
      absl::make_unique<std::map<std::string, SigningV4Test>>();
  google::cloud::storage::signing_tests = signing_tests_destroyer.get();

  // The implementation is not yet completed and these tests still fail, so skip
  // them so far.
  std::set<std::string> nonconformant_url_tests{"ListObjects"};

  for (auto const& signing_test : tests.signing_v4_tests()) {
    std::string name_with_spaces = signing_test.description();
    std::string name;
    // gtest doesn't allow for anything other than [a-zA-Z]
    std::copy_if(name_with_spaces.begin(), name_with_spaces.end(),
                 back_inserter(name), [](char c) {
                   return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
                 });
    if (nonconformant_url_tests.find(name) != nonconformant_url_tests.end()) {
      continue;
    }
    bool inserted =
        google::cloud::storage::signing_tests->emplace(name, signing_test)
            .second;
    if (!inserted) {
      std::cerr << "Duplicate test description: " << name << "\n";
    }
  }

  auto post_policy_tests_destroyer =
      absl::make_unique<std::map<std::string, PostPolicyV4Test>>();
  google::cloud::storage::post_policy_tests = post_policy_tests_destroyer.get();

  for (auto const& policy_test : tests.post_policy_v4_tests()) {
    std::string name_with_spaces = policy_test.description();
    std::string name;
    // gtest doesn't allow for anything other than [a-zA-Z]
    std::copy_if(name_with_spaces.begin(), name_with_spaces.end(),
                 back_inserter(name), [](char c) {
                   return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
                 });
#if !GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    if (name == "POSTPolicyCharacterEscaping" ||
        name == "POSTPolicyWithAdditionalMetadata") {
      // Escaping is not supported if exceptions are unavailable.
      continue;
    }
#endif  // !GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    bool inserted =
        google::cloud::storage::post_policy_tests->emplace(name, policy_test)
            .second;
    if (!inserted) {
      std::cerr << "Duplicate test description: " << name << "\n";
    }
  }

  ::testing::InitGoogleMock(&argc, argv);

  return RUN_ALL_TESTS();
}
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
