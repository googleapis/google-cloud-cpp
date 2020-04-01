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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/internal/signed_url_requests.h"
#include "google/cloud/storage/list_objects_reader.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/terminate_handler.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/init_google_mock.h"
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
using ::testing::HasSubstr;

// Initialized in main() below.
std::map<std::string, internal::nl::json>* tests;

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

  std::vector<std::pair<std::string, std::string>> ExtractHeaders(
      internal::nl::json j_obj) {
    return ExtractListOfPairs(std::move(j_obj), "headers");
  }

  std::vector<std::pair<std::string, std::string>> ExtractQueryParams(
      internal::nl::json j_obj) {
    return ExtractListOfPairs(std::move(j_obj), "queryParameters");
  }

  std::string service_account_key_filename_;

 private:
  std::vector<std::pair<std::string, std::string>> ExtractListOfPairs(
      internal::nl::json j_obj, std::string const& field) {
    std::vector<std::pair<std::string, std::string>> res;

    // Check for the keys of the relevant field
    for (auto& x : j_obj[field].items()) {
      // The keys are returned in alphabetical order by nlohmann::json, but
      // the order does not matter when creating signed urls.
      res.emplace_back(x.key(), x.value());
    }
    return res;
  }
};

TEST_P(V4SignedUrlConformanceTest, V4SignJson) {
  auto creds = oauth2::CreateServiceAccountCredentialsFromJsonFilePath(
      service_account_key_filename_);

  ASSERT_STATUS_OK(creds);
  std::string account_email = creds->get()->AccountEmail();
  Client client(*creds);
  std::string actual_canonical_request;
  std::string actual_string_to_sign;

  auto j_obj = (*tests)[GetParam()];
  std::string const method_name = j_obj["method"];
  std::string const bucket_name = j_obj["bucket"];
  std::string const object_name = j_obj["object"];
  std::string const scheme = j_obj["scheme"];
  std::string url_style;
  if (j_obj.count("urlStyle") > 0) {
    url_style = j_obj["urlStyle"];
  }
  std::string const date = j_obj["timestamp"];
  auto const valid_for =
      std::chrono::seconds(std::stoi(j_obj["expiration"].get<std::string>()));
  std::string const expected = j_obj["expectedUrl"];
  std::string const expected_canonical_request =
      j_obj["expectedCanonicalRequest"];
  std::string const expected_string_to_sign = j_obj["expectedStringToSign"];

  // Extract the headers for each object
  auto headers = ExtractHeaders(j_obj);
  auto params = ExtractQueryParams(j_obj);

  google::cloud::storage::internal::V4SignUrlRequest request(
      method_name, bucket_name, object_name);
  request.set_multiple_options(
      SignedUrlTimestamp(google::cloud::internal::ParseRfc3339(date)),
      SignedUrlDuration(valid_for));

  std::vector<AddExtensionHeaderOption> header_extensions(5);
  ASSERT_LE(headers.size(), header_extensions.size());
  for (std::size_t i = 0; i < headers.size(); ++i) {
    auto& header = headers.at(i);
    request.set_multiple_options(
        AddExtensionHeader(header.first, header.second));
    header_extensions[i] = AddExtensionHeader(header.first, header.second);
  }

  std::vector<AddQueryParameterOption> query_params(5);
  ASSERT_LE(params.size(), query_params.size());
  for (std::size_t i = 0; i < params.size(); ++i) {
    auto& param = params.at(i);
    request.set_multiple_options(
        AddQueryParameterOption(param.first, param.second));
    query_params[i] = AddQueryParameterOption(param.first, param.second);
  }

  VirtualHostname virtual_hotname;
  if (url_style == "VIRTUAL_HOSTED_STYLE") {
    virtual_hotname = VirtualHostname(true);
    request.set_multiple_options(VirtualHostname(true));
  }

  BucketBoundHostname domain_named_bucket;
  if (url_style == "BUCKET_BOUND_HOSTNAME") {
    domain_named_bucket = BucketBoundHostname(j_obj["bucketBoundHostname"]);
    request.set_multiple_options(
        BucketBoundHostname(j_obj["bucketBoundHostname"]));
  }

  auto actual = client.CreateV4SignedUrl(
      method_name, bucket_name, object_name,
      SignedUrlTimestamp(google::cloud::internal::ParseRfc3339(date)),
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
      std::transform(tests->begin(), tests->end(), std::back_inserter(res),
                     [](std::pair<std::string, internal::nl::json> const& p) {
                       return p.first;
                     });
      return res;
    }()));

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
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

  auto tests_destroyer = google::cloud::internal::make_unique<
      std::map<std::string, google::cloud::storage::internal::nl::json>>();
  google::cloud::storage::tests = tests_destroyer.get();

  // The implementation is not yet completed and these tests still fail, so skip
  // them so far.
  std::set<std::string> nonconformant_tests{"ListObjects",
                                            "POSTPolicyACLmatching",
                                            "POSTPolicyCacheControlFileHeader",
                                            "POSTPolicySimple",
                                            "POSTPolicySuccessWithRedirect",
                                            "POSTPolicySuccessWithStatus",
                                            "POSTPolicyWithinContentRange"};

  auto json = google::cloud::storage::internal::nl::json::parse(ifstr);
  if (json.is_discarded()) {
    std::cerr << "Failed to parse provided data file\n";
    return 1;
  }

  if (!json.is_object()) {
    std::cerr << "The provided file should contain one JSON object.\n";
    return 1;
  }
  if (json.count("signingV4Tests") < 1) {
    std::cerr << "The provided file should contain a 'signingV4Tests' entry.\n"
              << json;
    return 1;
  }

  auto signing_tests_json = json["signingV4Tests"];
  if (!signing_tests_json.is_array()) {
    std::cerr << "Expected an obects' value to be arrays, found: "
              << signing_tests_json << ".\n";
    return 1;
  }

  if (!signing_tests_json.is_array()) {
    std::cerr << "Expected an obects' value to be arrays, found: "
              << signing_tests_json << ".\n";
    return 1;
  }

  for (auto const& j_obj : signing_tests_json) {
    if (!j_obj.is_object()) {
      std::cerr << "Expected an array of objects, got this element in array: "
                << j_obj << "\n";
      return 1;
    }
    if (j_obj.count("description") != 1) {
      std::cerr << "Expected all tests to have a description\n";
      return 1;
    }
    auto j_descr = j_obj["description"];
    if (!j_descr.is_string()) {
      std::cerr << "Expected description to be a string, got: " << j_descr
                << "\n";
      return 1;
    }
    std::string name_with_spaces = j_descr;
    std::string name;
    // gtest doesn't allow for anything other than [a-zA-Z]
    std::copy_if(name_with_spaces.begin(), name_with_spaces.end(),
                 back_inserter(name), [](char c) {
                   return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
                 });
    if (nonconformant_tests.find(name) != nonconformant_tests.end()) {
      continue;
    }
    bool inserted = google::cloud::storage::tests->emplace(name, j_obj).second;
    if (!inserted) {
      std::cerr << "Duplicate test description: " << name << "\n";
    }
  }
  google::cloud::testing_util::InitGoogleMock(argc, argv);

  return RUN_ALL_TESTS();
}
