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
#include <cctype>
#include <fstream>
#include <streambuf>

namespace nl = google::cloud::storage::internal::nl;

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
using ::testing::HasSubstr;

// Initialized in main() below.
char const* account_file_name_;
char const* data_file_name_;

class ObjectIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  // Converts ""20190201T090000Z",
  // to  "2019-02-01T09:00:00Z"
  std::string TimestampToRfc3339(std::string ts) {
    if (ts.size() != 16) {
      return "";
    }
    return ts.substr(0, 4) + '-' + ts.substr(4, 2) + '-' + ts.substr(6, 2) +
           'T' + ts.substr(9, 2) + ':' + ts.substr(11, 2) + ':' +
           ts.substr(13, 2) + 'Z';
  }

  std::vector<std::pair<std::string, std::string>> ExtractHeaders(
      nl::json j_obj) {
    std::vector<std::pair<std::string, std::string>> headers;

    for (auto& header : j_obj.items()) {
      std::string header_key = header.key();

      if (header_key == "headers") {
        std::string key_name;
        std::string value_name;
        // Check for the keys of the headers field
        for (auto& x : j_obj["headers"].items()) {
          // The keys are being outputted in alphabetical order
          // not in the order they are in the file

          //  Inside the file
          //        "headers": {
          //            "foo":  "foo-value" ,
          //            "BAR":  "BAR-value"
          //        },

          // Output by nlohman library
          //        "headers": {
          //            "BAR":  "BAR-value" ,
          //            "foo":  "foo-value"
          //        },

          key_name = x.key();
          value_name = x.value();
          headers.emplace_back(key_name, value_name);
        }
      }
    }
    return headers;
  }
};

TEST_F(ObjectIntegrationTest, V4SignJson) {
  // This is a dummy service account JSON file that is inactive. It's fine for
  // it to be public.
  std::string account_file = account_file_name_;
  std::string data_file = data_file_name_;

  std::ifstream ifstr(data_file);
  if (!ifstr.is_open()) {
    // If the file does not exist, or
    // if we were unable to open it for some other reason.
    std::cout << "Cannot open credentials file " + data_file + '\n';
    return;
  }
  // nl::json json_array;
  nl::json json_array = nl::json::parse(ifstr);
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

    int validInt = j_obj["expiration"];
    auto const valid_for = std::chrono::seconds(validInt);
    std::string const expected = j_obj["expectedUrl"];

    // Extract headers for each object
    std::vector<std::pair<std::string, std::string>> headers =
        ExtractHeaders(j_obj);

    if (headers.size() == 0) {
      actual = client.CreateV4SignedUrl(
          method_name, bucket_name, object_name,
          SignedUrlTimestamp(internal::ParseRfc3339(date)),
          SignedUrlDuration(valid_for),
          AddExtensionHeader("host", "storage.googleapis.com"));
      if (object_name != "") {
        std::cout << "No Headers  "
                  << "Description: " << j_obj["description"] << '\n';
        ASSERT_STATUS_OK(actual);
        EXPECT_THAT(*actual, HasSubstr(bucket_name));
        EXPECT_THAT(*actual, HasSubstr(object_name));
        EXPECT_EQ(expected, *actual);
      }
    } else if (headers.size() > 0) {
      if (headers.size() == 1) {
        std::cout << "Headers 1  "
                  << "Description: " << j_obj["description"] << '\n';
        if (headers.front().second != "ignored") {
          actual = client.CreateV4SignedUrl(
              method_name, bucket_name, object_name,
              SignedUrlTimestamp(internal::ParseRfc3339(date)),
              SignedUrlDuration(valid_for),
              AddExtensionHeader("host", "storage.googleapis.com"),
              AddExtensionHeader(headers.at(0).first, headers.at(0).second));
        }
      } else if (headers.size() == 2) {
        std::cout << "Headers 2  "
                  << "Description: " << j_obj["description"] << '\n';

        // For some reason the function outputs them as a map
        actual = client.CreateV4SignedUrl(
            method_name, bucket_name, object_name,
            SignedUrlTimestamp(internal::ParseRfc3339(date)),
            SignedUrlDuration(valid_for),
            AddExtensionHeader("host", "storage.googleapis.com"),
            AddExtensionHeader(headers.at(0).first, headers.at(0).second),
            AddExtensionHeader(headers.at(1).first, headers.at(1).second));
      } else if (headers.size() == 3) {
        std::cout << "Headers 3  "
                  << "Description: " << j_obj["description"] << '\n';

        actual = client.CreateV4SignedUrl(
            method_name, bucket_name, object_name,
            SignedUrlTimestamp(internal::ParseRfc3339(date)),
            SignedUrlDuration(valid_for),
            AddExtensionHeader("host", "storage.googleapis.com"),
            AddExtensionHeader(headers.at(0).first, headers.at(0).second),
            AddExtensionHeader(headers.at(1).first, headers.at(1).second),
            AddExtensionHeader(headers.at(2).first, headers.at(2).second));
      }
      ASSERT_STATUS_OK(actual);
      EXPECT_THAT(*actual, HasSubstr(bucket_name));
      EXPECT_THAT(*actual, HasSubstr(object_name));
      EXPECT_EQ(expected, *actual);
    }
    EXPECT_LT(headers.size(), 4);
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
              << " <key-file-name> <all-file-name>\n";
    return 1;
  }

  google::cloud::storage::account_file_name_ = argv[1];
  google::cloud::storage::data_file_name_ = argv[2];

  return RUN_ALL_TESTS();
}
