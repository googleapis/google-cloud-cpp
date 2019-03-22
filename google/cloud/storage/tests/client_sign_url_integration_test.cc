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
  std::string trimmedString(std::string data_file) {
    std::ifstream ifstr(data_file);
    if (!ifstr.is_open()) {
      // If the file does not exist, or
      // if we were unable to open it for some other reason.
      std::cout << "Cannot open credentials file " + data_file + '\n';
      return "";
    }

    std::string from_file((std::istreambuf_iterator<char>(ifstr)),
                          std::istreambuf_iterator<char>());

    // int from_file_len = from_file.length();
    size_t from_file_len = from_file.size();
    std::string trimmed;
    char const slash = '/';

    // Check whether a character is
    // within //.............\n
    bool check_comment = false;

    for (std::string::size_type i = 0; i < from_file_len; i++) {
      // Check for //
      if (i == 0 && from_file[i] == slash && from_file[i + 1] == slash) {
        i++;
        check_comment = true;
      }

      // Check for the end of the commented line \n
      else if (check_comment == true && from_file[i] == '\n') {
        i++;
        check_comment = false;
      }

      // Check for // only and not for ://
      // Do not want to remove https://
      if (from_file[i] == slash && from_file[i + 1] == slash &&
          from_file[i - 1] != ':') {
        i++;
        check_comment = true;
      }

      else if (check_comment == false) {
        trimmed += from_file[i];
      }
    }

    // Remove the last comma from the string
    //  }, ] if any
    int trimmed_len = trimmed.length();
    std::string final_trimmed_string;

    int final_comma_position = -1;
    char const square_bracket = ']';
    char const comma = ',';
    char const brace = '}';
    bool comma_check = false;

    for (int i = trimmed_len - 1; i > 0; i--) {
      if (std::isspace(trimmed[i])) {
        comma_check = true;
      } else if (trimmed[i] == square_bracket && comma_check == true) {
        i--;
      } else if (trimmed[i] == comma && comma_check == true) {
        final_comma_position = i;
        break;
      } else if (trimmed[i] == brace && comma_check == true) {
        break;
      } else if (!std::isspace(trimmed[i])) {
        comma_check = false;
      }
    }

    if (final_comma_position > -1) {
      for (int i = 0; i < trimmed_len; i++) {
        if (i != final_comma_position) {
          final_trimmed_string += trimmed[i];
        }
      }
      return final_trimmed_string;
    }
    return trimmed;
  }

  std::string TimestampToRfc3339(std::string ts) {
    if (ts.size() != 16) {
      return "";
    }
    return ts.substr(0, 4) + '-' + ts.substr(4, 2) + '-' + ts.substr(6, 2) +
           'T' + ts.substr(9, 2) + ':' + ts.substr(11, 2) + ':' +
           ts.substr(13, 2) + 'Z';
  }

  std::vector<std::pair<std::string, std::vector<std::string>>> ExtractHeaders(
      nl::json j_obj) {
    std::vector<std::pair<std::string, std::vector<std::string>>> headers;

    for (auto& header : j_obj.items()) {
      std::string header_key = header.key();

      if (header_key == "headers") {
        // The size of the headers obj, how many keys it has.
        // 1 one key
        // 2 two keys or 3 keys
        // Each key has an array of 1 or more strings

        std::string key_name;

        // Check for the keys of the headers field
        for (auto const& x : j_obj["headers"].items()) {
          // The keys are being outputted in alphabetical order
          // not in the order they are in the file
          key_name = x.key();
          std::vector<std::string> value_array;
          for (auto const& f : x.value()) {
            value_array.emplace_back(f);
          }
          headers.emplace_back(key_name, value_array);
        }
      }
    }
    return headers;
  }
};

// Testing all the objects
// Headers and non headers

// Without headers
// No "List Objects" the object field is empty
// description: "Simple GET"
// description: "Simple PUT"
// description: "Vary expiration and timestamp"
// description: "Vary bucket and object"

// No "Trimming of multiple header values"
// No "Multi-value headers"

// as per original file
// https://github.com/googleapis/google-cloud-dotnet/blob/e918df5a988f53ed71cebf708a0dd06bed8bef43/apis/Google.Cloud.Storage.V1/Google.Cloud.Storage.V1.Tests/UrlSignerV4TestData.json#L42

// With headers
// description:  "POST for resumable uploads"
// description:  "Simple headers"
// description:  "Headers should be trimmed"

// With headers  key : [ "ignored" ]
// "Customer-supplied encryption key"

TEST_F(ObjectIntegrationTest, V4SignJson) {
  // This is a dummy service account JSON file that is inactive. It's fine for
  // it to be public.
  std::string account_file = account_file_name_;
  std::string data_file = data_file_name_;

  auto trimmed_string = trimmedString(data_file);
  if (trimmed_string.size() == 0) {
    return;
  }

  nl::json json_array = nl::json::parse(trimmed_string);

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

    // ExtractHeaders for each object
    // ExtractHeaders(j_obj) for each object

    std::vector<std::pair<std::string, std::vector<std::string>>> headers =
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
    }

    else if (headers.size() > 0 && headers.front().second.size() == 1) {
      if (headers.size() == 1) {
        std::cout << "Headers 1  "
                  << "Description: " << j_obj["description"] << '\n';
        if (headers.front().second.at(0) != "ignored") {
          actual = client.CreateV4SignedUrl(
              method_name, bucket_name, object_name,
              SignedUrlTimestamp(internal::ParseRfc3339(date)),
              SignedUrlDuration(valid_for),
              AddExtensionHeader("host", "storage.googleapis.com"),
              AddExtensionHeader(headers.at(0).first,
                                 headers.at(0).second.at(0)));
        }

        ASSERT_STATUS_OK(actual);
        EXPECT_THAT(*actual, HasSubstr(bucket_name));
        EXPECT_THAT(*actual, HasSubstr(object_name));
        EXPECT_EQ(expected, *actual);
      }

      else if (headers.size() == 2) {
        std::cout << "Headers 2  "
                  << "Description: " << j_obj["description"] << '\n';

        // For some reason the function outputs them as a map
        actual = client.CreateV4SignedUrl(
            method_name, bucket_name, object_name,
            SignedUrlTimestamp(internal::ParseRfc3339(date)),
            SignedUrlDuration(valid_for),
            AddExtensionHeader("host", "storage.googleapis.com"),
            AddExtensionHeader(headers.at(1).first, headers.at(1).second.at(0)),
            AddExtensionHeader(headers.at(0).first,
                               headers.at(0).second.at(0)));

        ASSERT_STATUS_OK(actual);
        EXPECT_THAT(*actual, HasSubstr(bucket_name));
        EXPECT_THAT(*actual, HasSubstr(object_name));
        EXPECT_EQ(expected, *actual);

      }

      else if (headers.size() == 3 &&
               headers.front().second.at(0) != "ignored") {
        std::cout << "Headers 2  "
                  << "Description: " << j_obj["description"] << '\n';

        actual = client.CreateV4SignedUrl(
            method_name, bucket_name, object_name,
            SignedUrlTimestamp(internal::ParseRfc3339(date)),
            SignedUrlDuration(valid_for),
            AddExtensionHeader("host", "storage.googleapis.com"),
            AddExtensionHeader(headers.at(1).first, headers.at(1).second.at(0)),
            AddExtensionHeader(headers.at(2).first, headers.at(2).second.at(0)),
            AddExtensionHeader(headers.at(0).first,
                               headers.at(0).second.at(0)));

        ASSERT_STATUS_OK(actual);
        EXPECT_THAT(*actual, HasSubstr(bucket_name));
        EXPECT_THAT(*actual, HasSubstr(object_name));
        EXPECT_EQ(expected, *actual);
      }

      else if (headers.size() == 3 &&
               headers.front().second.at(0) == "ignored") {
        std::cout << "Headers 2  "
                  << "Description: " << j_obj["description"] << '\n';

        actual = client.CreateV4SignedUrl(
            method_name, bucket_name, object_name,
            SignedUrlTimestamp(internal::ParseRfc3339(date)),
            SignedUrlDuration(valid_for),
            AddExtensionHeader("host", "storage.googleapis.com"));

        ASSERT_STATUS_OK(actual);
        EXPECT_THAT(*actual, HasSubstr(bucket_name));
        EXPECT_THAT(*actual, HasSubstr(object_name));
        EXPECT_EQ(expected, *actual);
      }
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
