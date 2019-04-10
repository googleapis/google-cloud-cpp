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
  // Converts ""20190201T090000Z",
  // to  "2019-02-01T09:00:00Z"
  std::string TimestampToRfc3339(std::string const& ts) {
    if (ts.size() != 16) {
      return "";
    }
    return ts.substr(0, 4) + '-' + ts.substr(4, 2) + '-' + ts.substr(6, 2) +
           'T' + ts.substr(9, 2) + ':' + ts.substr(11, 2) + ':' +
           ts.substr(13, 2) + 'Z';
  }

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
  Client client(*creds);
  StatusOr<std::string> actual;

  for (auto const& j_obj : json_array) {
    std::string const method_name = j_obj["method"];
    std::string const bucket_name = j_obj["bucket"];
    std::string const object_name = j_obj["object"];
    std::string const date = TimestampToRfc3339(j_obj["timestamp"]);
    auto const valid_for = std::chrono::seconds(j_obj["expiration"]);
    std::string const expected = j_obj["expectedUrl"];

    // Extract the headers for each object
    std::vector<std::pair<std::string, std::string>> headers =
        ExtractHeaders(j_obj);

    if (headers.empty()) {
      actual = client.CreateV4SignedUrl(
          method_name, bucket_name, object_name,
          SignedUrlTimestamp(internal::ParseRfc3339(date)),
          SignedUrlDuration(valid_for),
          AddExtensionHeader("host", "storage.googleapis.com"));
    } else if (headers.size() == 1) {
      actual = client.CreateV4SignedUrl(
          method_name, bucket_name, object_name,
          SignedUrlTimestamp(internal::ParseRfc3339(date)),
          SignedUrlDuration(valid_for),
          AddExtensionHeader("host", "storage.googleapis.com"),
          AddExtensionHeader(headers.at(0).first, headers.at(0).second));
    } else if (headers.size() == 2) {
      actual = client.CreateV4SignedUrl(
          method_name, bucket_name, object_name,
          SignedUrlTimestamp(internal::ParseRfc3339(date)),
          SignedUrlDuration(valid_for),
          AddExtensionHeader("host", "storage.googleapis.com"),
          AddExtensionHeader(headers.at(0).first, headers.at(0).second),
          AddExtensionHeader(headers.at(1).first, headers.at(1).second));
    } else if (headers.size() == 3) {
      actual = client.CreateV4SignedUrl(
          method_name, bucket_name, object_name,
          SignedUrlTimestamp(internal::ParseRfc3339(date)),
          SignedUrlDuration(valid_for),
          AddExtensionHeader("host", "storage.googleapis.com"),
          AddExtensionHeader(headers.at(0).first, headers.at(0).second),
          AddExtensionHeader(headers.at(1).first, headers.at(1).second),
          AddExtensionHeader(headers.at(2).first, headers.at(2).second));
    } else {
      EXPECT_LT(headers.size(), 4);
    }
    ASSERT_STATUS_OK(actual);
    EXPECT_THAT(*actual, HasSubstr(bucket_name));
    EXPECT_THAT(*actual, HasSubstr(object_name));
    EXPECT_EQ(expected, *actual);
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
