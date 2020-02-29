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
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>
#include <fstream>

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
char const* account_file_name;
char const* data_file_name;

class V4SignedUrlConformanceTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  std::vector<std::pair<std::string, std::string>> ExtractHeaders(
      internal::nl::json j_obj) {
    std::vector<std::pair<std::string, std::string>> headers;

    // Check for the keys of the headers field
    for (auto& x : j_obj["headers"].items()) {
      // The keys are returned in alphabetical order by nlohmann::json, but
      // the order does not matter when creating signed urls.
      headers.emplace_back(x.key(), x.value());
    }
    return headers;
  }
};

TEST_F(V4SignedUrlConformanceTest, V4SignJson) {
  std::string account_file = account_file_name;
  std::string data_file = data_file_name;

  std::ifstream ifstr(data_file);
  ASSERT_TRUE(ifstr.is_open());

  auto json_array = internal::nl::json::parse(ifstr);
  auto creds =
      oauth2::CreateServiceAccountCredentialsFromJsonFilePath(account_file);

  ASSERT_STATUS_OK(creds);
  std::string account_email = creds->get()->AccountEmail();
  Client client(*creds);
  StatusOr<std::string> actual;
  std::string actual_canonical_request;
  std::string actual_string_to_sign;

  for (auto const& j_obj : json_array) {
    std::string const method_name = j_obj["method"];
    std::string const bucket_name = j_obj["bucket"];
    std::string const object_name = j_obj["object"];
    std::string const date = j_obj["timestamp"];
    auto const valid_for =
        std::chrono::seconds(std::stoi(j_obj["expiration"].get<std::string>()));
    std::string const expected = j_obj["expectedUrl"];
    std::string const expected_canonical_request =
        j_obj["expectedCanonicalRequest"];
    std::string const expected_string_to_sign = j_obj["expectedStringToSign"];

    // Extract the headers for each object
    std::vector<std::pair<std::string, std::string>> headers =
        ExtractHeaders(j_obj);

    google::cloud::storage::internal::V4SignUrlRequest request(
        method_name, bucket_name, object_name);
    if (headers.empty()) {
      request.set_multiple_options(
          SignedUrlTimestamp(google::cloud::internal::ParseRfc3339(date)),
          SignedUrlDuration(valid_for),
          AddExtensionHeader("host", "storage.googleapis.com"));

      actual = client.CreateV4SignedUrl(
          method_name, bucket_name, object_name,
          SignedUrlTimestamp(google::cloud::internal::ParseRfc3339(date)),
          SignedUrlDuration(valid_for),
          AddExtensionHeader("host", "storage.googleapis.com"));
    } else if (headers.size() == 1) {
      request.set_multiple_options(
          SignedUrlTimestamp(google::cloud::internal::ParseRfc3339(date)),
          SignedUrlDuration(valid_for),
          AddExtensionHeader("host", "storage.googleapis.com"),
          AddExtensionHeader(headers.at(0).first, headers.at(0).second));

      actual = client.CreateV4SignedUrl(
          method_name, bucket_name, object_name,
          SignedUrlTimestamp(google::cloud::internal::ParseRfc3339(date)),
          SignedUrlDuration(valid_for),
          AddExtensionHeader("host", "storage.googleapis.com"),
          AddExtensionHeader(headers.at(0).first, headers.at(0).second));
    } else if (headers.size() == 2) {
      request.set_multiple_options(
          SignedUrlTimestamp(google::cloud::internal::ParseRfc3339(date)),
          SignedUrlDuration(valid_for),
          AddExtensionHeader("host", "storage.googleapis.com"),
          AddExtensionHeader(headers.at(0).first, headers.at(0).second),
          AddExtensionHeader(headers.at(1).first, headers.at(1).second));

      actual = client.CreateV4SignedUrl(
          method_name, bucket_name, object_name,
          SignedUrlTimestamp(google::cloud::internal::ParseRfc3339(date)),
          SignedUrlDuration(valid_for),
          AddExtensionHeader("host", "storage.googleapis.com"),
          AddExtensionHeader(headers.at(0).first, headers.at(0).second),
          AddExtensionHeader(headers.at(1).first, headers.at(1).second));
    } else if (headers.size() == 3) {
      request.set_multiple_options(
          SignedUrlTimestamp(google::cloud::internal::ParseRfc3339(date)),
          SignedUrlDuration(valid_for),
          AddExtensionHeader("host", "storage.googleapis.com"),
          AddExtensionHeader(headers.at(0).first, headers.at(0).second),
          AddExtensionHeader(headers.at(1).first, headers.at(1).second),
          AddExtensionHeader(headers.at(2).first, headers.at(2).second));

      actual = client.CreateV4SignedUrl(
          method_name, bucket_name, object_name,
          SignedUrlTimestamp(google::cloud::internal::ParseRfc3339(date)),
          SignedUrlDuration(valid_for),
          AddExtensionHeader("host", "storage.googleapis.com"),
          AddExtensionHeader(headers.at(0).first, headers.at(0).second),
          AddExtensionHeader(headers.at(1).first, headers.at(1).second),
          AddExtensionHeader(headers.at(2).first, headers.at(2).second));
    } else if (headers.size() == 4) {
      request.set_multiple_options(
          SignedUrlTimestamp(google::cloud::internal::ParseRfc3339(date)),
          SignedUrlDuration(valid_for),
          AddExtensionHeader("host", "storage.googleapis.com"),
          AddExtensionHeader(headers.at(0).first, headers.at(0).second),
          AddExtensionHeader(headers.at(1).first, headers.at(1).second),
          AddExtensionHeader(headers.at(2).first, headers.at(2).second),
          AddExtensionHeader(headers.at(3).first, headers.at(3).second));

      actual = client.CreateV4SignedUrl(
          method_name, bucket_name, object_name,
          SignedUrlTimestamp(google::cloud::internal::ParseRfc3339(date)),
          SignedUrlDuration(valid_for),
          AddExtensionHeader("host", "storage.googleapis.com"),
          AddExtensionHeader(headers.at(0).first, headers.at(0).second),
          AddExtensionHeader(headers.at(1).first, headers.at(1).second),
          AddExtensionHeader(headers.at(2).first, headers.at(2).second),
          AddExtensionHeader(headers.at(3).first, headers.at(3).second));
    } else {
      EXPECT_LT(headers.size(), 5);
    }

    actual_string_to_sign = request.StringToSign(account_email);
    actual_canonical_request = request.CanonicalRequest(account_email);

    ASSERT_STATUS_OK(actual);
    EXPECT_THAT(*actual, HasSubstr(bucket_name));
    EXPECT_EQ(expected, *actual);
    EXPECT_EQ(expected_canonical_request, actual_canonical_request);
    EXPECT_EQ(expected_string_to_sign, actual_string_to_sign);
  }
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InitGoogleMock(argc, argv);

  // Make sure the arguments are valid.
  if (argc != 3) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <key-file-name> <conformance-tests-json-file-name>\n";
    return 1;
  }

  google::cloud::storage::account_file_name = argv[1];
  google::cloud::storage::data_file_name = argv[2];

  return RUN_ALL_TESTS();
}
