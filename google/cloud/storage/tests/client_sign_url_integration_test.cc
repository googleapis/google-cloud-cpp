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

    int from_file_len = from_file.length();
    std::string trimmed;

    // Check whether a character is
    // within //.............\n
    bool check_comment = false;

    for (int i = 0; i < from_file_len; i++) {
      // Check for //
      if (i == 0 && from_file[i] == '/' && from_file[i + 1] == '/') {
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
      if (from_file[i] == '/' && from_file[i + 1] == '/' &&
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
    char square_bracket = ']';
    char comma = ',';
    char brace = '}';
    bool comma_check = false;

    for (int i = trimmed_len - 1; i > 0; i--) {
      if (isspace(trimmed[i])) {
        comma_check = true;
      } else if (trimmed[i] == square_bracket && comma_check == true) {
        i--;
      } else if (trimmed[i] == comma && comma_check == true) {
        final_comma_position = i;
        break;
      } else if (trimmed[i] == brace && comma_check == true) {
        break;
      } else if (!isspace(trimmed[i])) {
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

  std::string timestampString(std::string original_string) {
    // Turn  into  "20190201T090000Z"
    // int0 "2019-02-01T09:00:00Z"
    std::string timestamp_string;
    int len = original_string.length();

    for (int i = 0; i < len; i++) {
      timestamp_string += original_string[i];

      if (i == 3 || i == 5) {
        timestamp_string += '-';
      }
      if (i == 10 || i == 12) {
        timestamp_string += ':';
      }
    }
    return timestamp_string;
  }
};

// Headers and non headers

// Testing all the objects

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
TEST_F(ObjectIntegrationTest, V4SignStringAll) {
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

  for (auto const& j_obj : json_array) {
    std::string const method_name = j_obj["method"];  // GET
    std::string const bucket_name = j_obj["bucket"];
    std::string const object_name = j_obj["object"];
    std::string const date = timestampString(j_obj["timestamp"]);
    int validInt = j_obj["expiration"];
    auto const valid_for = std::chrono::seconds(validInt);
    std::string const expected = j_obj["expectedUrl"];

    std::string header_key;
    std::string has_headers = "no";

    // Check for the 'headers' field in each j_obj
    for (auto& header : j_obj.items()) {
      header_key = header.key();
      int array_size_per_key = 0;

      if (header_key == "headers") {
        has_headers = "yes";

        // The size of the headers obj, how many keys it has.
        // 1 one key
        // 2 two keys or 3 keys
        // Each key has an array of 1 or more strings

        std::string key_name;
        std::string header_name;

        std::vector<std::string> key_vector;
        std::vector<std::string> value_vector;

        // Check for the keys of the headers field
        for (auto& x : j_obj["headers"].items()) {
          // Each key
          key_name = x.key();
          key_vector.emplace_back(key_name);

          // Each key has an array as value
          array_size_per_key = x.value().size();

          // Only one value per key : key : [value]
          header_name = x.value()[0];
          value_vector.emplace_back(header_name);
        }

        // Only headers with key : [ value ]
        //              not  key : [ value1, value2 ]

        // No Customer-supplied encryption key
        // see the original file
        // All the fields in the header that have only one value per key
        if (array_size_per_key == 1 && value_vector.at(0) != "ignored") {
          std::cout << "Description: " << j_obj["description"] << '\n';

          // Check size of keys
          // key : [ value]
          if (key_vector.size() == 1) {
            auto actual = client.CreateV4SignedUrl(
                method_name, bucket_name, object_name,
                SignedUrlTimestamp(internal::ParseRfc3339(date)),
                SignedUrlDuration(valid_for),
                AddExtensionHeader("host", "storage.googleapis.com"),
                AddExtensionHeader(key_vector.at(0), value_vector.at(0)));

            ASSERT_STATUS_OK(actual);
            EXPECT_THAT(*actual, HasSubstr(bucket_name));
            EXPECT_THAT(*actual, HasSubstr(object_name));
            EXPECT_EQ(expected, *actual);

          }

          // 2     key : [value] 	pairs
          else if (key_vector.size() == 2) {
            auto actual = client.CreateV4SignedUrl(
                method_name, bucket_name, object_name,
                SignedUrlTimestamp(internal::ParseRfc3339(date)),
                SignedUrlDuration(valid_for),
                AddExtensionHeader("host", "storage.googleapis.com"),
                AddExtensionHeader(key_vector.at(0), value_vector.at(0)),
                AddExtensionHeader(key_vector.at(1), value_vector.at(1)));

            ASSERT_STATUS_OK(actual);
            EXPECT_THAT(*actual, HasSubstr(bucket_name));
            EXPECT_THAT(*actual, HasSubstr(object_name));
            EXPECT_EQ(expected, *actual);

          }

          // 3 key : [value ] pairs
          else if (key_vector.size() == 3) {
            auto actual = client.CreateV4SignedUrl(
                method_name, bucket_name, object_name,
                SignedUrlTimestamp(internal::ParseRfc3339(date)),
                SignedUrlDuration(valid_for),
                AddExtensionHeader("host", "storage.googleapis.com"),
                AddExtensionHeader(key_vector.at(0), value_vector.at(0)),
                AddExtensionHeader(key_vector.at(1), value_vector.at(1)),
                AddExtensionHeader(key_vector.at(2), value_vector.at(2)));

            ASSERT_STATUS_OK(actual);
            EXPECT_THAT(*actual, HasSubstr(bucket_name));
            EXPECT_THAT(*actual, HasSubstr(object_name));
            EXPECT_EQ(expected, *actual);
          }
        }

        //"Customer-supplied encryption key"
        //  key : ["ignored"]
        if (array_size_per_key == 1 && value_vector.at(0) == "ignored") {
          std::cout << "Description: " << j_obj["description"] << '\n';
          auto actual = client.CreateV4SignedUrl(
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
    }

    // description: "Simple GET"
    // description: "Simple PUT"
    // description: "Vary expiration and timestamp"
    // description: "Vary bucket and object"

    // No "List Objects" , object = ""
    if (has_headers == "no" && object_name != "") {
      std::cout << "Description: " << j_obj["description"] << '\n';

      auto actual = client.CreateV4SignedUrl(
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
