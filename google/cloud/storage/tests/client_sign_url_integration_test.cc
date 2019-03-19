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
    : public google::cloud::storage::testing::StorageIntegrationTest {};

// Testing the objects with the 'headers' field only

// No "Trimming of multiple header values"
// No "Multi-value headers"

// as per original file
// https://github.com/googleapis/google-cloud-dotnet/blob/e918df5a988f53ed71cebf708a0dd06bed8bef43/apis/Google.Cloud.Storage.V1/Google.Cloud.Storage.V1.Tests/UrlSignerV4TestData.json#L42

// Description:  "POST for resumable uploads"
// Description:  "Simple headers"
// Description:  "Headers should be trimmed"

TEST_F(ObjectIntegrationTest, V4SignWithHeaders) {
  // This test uses a disabled key to create a V4 Signed URL for a GET
  // operation. The bucket name was generated at random too.

  // This is a dummy service account JSON file that is inactive. It's fine for
  // it to be public.
  std::string account_file = account_file_name_;
  std::string data_file = data_file_name_;

  auto creds =
      oauth2::CreateServiceAccountCredentialsFromJsonFilePath(account_file);

  ASSERT_STATUS_OK(creds);
  Client client(*creds);

  std::ifstream ifstr(data_file);
  if (!ifstr.is_open()) {
    // If the file does not exist, or
    // if we were unable to open it for some other reason.
    std::cout << "Cannot open credentials file " + data_file + '\n';
    return;
  }

  nl::json json_array = nl::json::parse(ifstr);

  for (auto const& j_obj : json_array) {
    std::string const method_name = j_obj["method"];  // GET
    std::string const bucket_name = j_obj["bucket"];
    std::string const object_name = j_obj["object"];
    std::string const date = j_obj["timestamp"];
    int validInt = j_obj["expiration"];
    auto const valid_for = std::chrono::seconds(validInt);
    std::string const expected = j_obj["expectedUrl"];

    // Check for headers field in each j_obj
    for (auto& header : j_obj.items()) {
      int array_size_per_key = 0;

      if (header.key() == "headers") {
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
        if (array_size_per_key == 1 &&
            j_obj["description"] != "Customer-supplied encryption key") {
          std::cout << "Description: "
                    << " " << j_obj["description"] << '\n';

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
      }
    }
  }
}

// Testing all the objects without the 'headers' key
// No "List Objects"
// description: "Simple GET"
// description: "Simple PUT"
// description: "Vary expiration and timestamp"
// description: "Vary bucket and object"

TEST_F(ObjectIntegrationTest, V4SignNoHeaders) {
  // This test uses a disabled key to create a V4 Signed URL for a GET
  // operation. The bucket name was generated at random too.

  // This is a dummy service account JSON file that is inactive. It's fine for
  // it to be public.
  std::string account_file = account_file_name_;
  std::string data_file = data_file_name_;

  auto creds =
      oauth2::CreateServiceAccountCredentialsFromJsonFilePath(account_file);

  ASSERT_STATUS_OK(creds);
  Client client(*creds);

  std::ifstream ifstr(data_file);
  if (!ifstr.is_open()) {
    // If the file does not exist, or
    // if we were unable to open it for some other reason.
    std::cout << "Cannot open credentials file " + data_file + '\n';
    return;
  }

  // nl::json json_array;
  nl::json json_array = nl::json::parse(ifstr);

  for (auto const& j_obj : json_array) {
    std::string const method_name = j_obj["method"];  // GET
    std::string const bucket_name = j_obj["bucket"];
    std::string const object_name = j_obj["object"];
    std::string const date = j_obj["timestamp"];
    int validInt = j_obj["expiration"];
    auto const valid_for = std::chrono::seconds(validInt);
    std::string const expected = j_obj["expectedUrl"];

    std::string header_key;
    std::string has_headers = "no";

    // Check for the 'headers' field in each j_obj
    for (auto& header : j_obj.items()) {
      header_key = header.key();
      // std::cout << header_key << '\n';

      if (header_key == "headers") {
        has_headers = "yes";
      }
    }

    // No "List Objects"
    // description: "Simple GET"
    // description: "Simple PUT"
    // description: "Vary expiration and timestamp"
    // description: "Vary bucket and object"

    if (has_headers == "no" && j_obj["description"] != "List Objects") {
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

// Customer-supplied encryption key
// works without using headers
// as mentioned in the original file
// https://github.com/googleapis/google-cloud-dotnet/blob/e918df5a988f53ed71cebf708a0dd06bed8bef43/apis/Google.Cloud.Storage.V1/Google.Cloud.Storage.V1.Tests/UrlSignerV4TestData.json#L42
TEST_F(ObjectIntegrationTest, V4SignGetCustomerSupplied) {
  // This test uses a disabled key to create a V4 Signed URL for a GET
  // operation. The bucket name was generated at random too.

  // This is a dummy service account JSON file that is inactive. It's fine for
  // it to be public.
  std::string account_file = account_file_name_;
  std::string data_file = data_file_name_;

  auto creds =
      oauth2::CreateServiceAccountCredentialsFromJsonFilePath(account_file);

  ASSERT_STATUS_OK(creds);
  Client client(*creds);

  std::ifstream ifstr(data_file);
  if (!ifstr.is_open()) {
    // If the file does not exist, or
    // if we were unable to open it for some other reason.
    std::cout << "Cannot open credentials file " + data_file + '\n';
    return;
  }

  // nl::json json_array;
  nl::json json_array = nl::json::parse(ifstr);

  for (auto const& j_obj : json_array) {
    std::string method_description = j_obj["description"];

    if (j_obj["method"] == "GET" &&
        method_description == "Customer-supplied encryption key") {
      std::cout << "Description: " << method_description << '\n';
      std::string const method_name = j_obj["method"];  // GET
      std::string const bucket_name = j_obj["bucket"];
      std::string const object_name = j_obj["object"];
      std::string const date = j_obj["timestamp"];
      int validInt = j_obj["expiration"];
      auto const valid_for = std::chrono::seconds(validInt);

      auto actual = client.CreateV4SignedUrl(
          method_name, bucket_name, object_name,
          SignedUrlTimestamp(internal::ParseRfc3339(date)),
          SignedUrlDuration(valid_for),
          AddExtensionHeader("host", "storage.googleapis.com"));
      ASSERT_STATUS_OK(actual);

      EXPECT_THAT(*actual, HasSubstr(bucket_name));
      EXPECT_THAT(*actual, HasSubstr(object_name));

      std::string const expected = j_obj["expectedUrl"];
      EXPECT_EQ(expected, *actual);
    }
  }
}

/*
//This does not work the object is empty
//description : "List Objects"
TEST_F(ObjectIntegrationTest, V4SignGetListObjects) {
  // This test uses a disabled key to create a V4 Signed URL for a GET
  // operation. The bucket name was generated at random too.

  // This is a dummy service account JSON file that is inactive. It's fine for
  // it to be public.
  std::string account_file = account_file_name_;
  std::string data_file = data_file_name_;

  auto creds =
      oauth2::CreateServiceAccountCredentialsFromJsonFilePath(account_file);

  ASSERT_STATUS_OK(creds);
  Client client(*creds);

  std::ifstream ifstr(data_file);
  if (!ifstr.is_open()) {
    // If the file does not exist, or
    // if we were unable to open it for some other reason.
    std::cout << "Cannot open credentials file " + data_file + '\n';
    return;
  }

  //nl::json json_array;
  nl::json json_array = nl::json::parse(ifstr);

  for (auto const& j_obj : json_array) {
    if (j_obj["method"] == "GET" && j_obj["description"] == "List Objects") {

         std::cout << j_obj["description"] << '\n';

      std::string const method_name = j_obj["method"];  // GET
      std::string const bucket_name = j_obj["bucket"];
      std::string const object_name = j_obj["object"];
      std::string const date = j_obj["timestamp"];
      int validInt = j_obj["expiration"];
      auto const valid_for = std::chrono::seconds(validInt);


      auto actual = client.CreateV4SignedUrl(
          method_name, bucket_name, object_name,
          SignedUrlTimestamp(internal::ParseRfc3339(date)),
          SignedUrlDuration(valid_for),
          AddExtensionHeader("host", "storage.googleapis.com")
          );

      ASSERT_STATUS_OK(actual);

      EXPECT_THAT(*actual, HasSubstr(bucket_name));
      EXPECT_THAT(*actual, HasSubstr(object_name));

      std::string const expected = j_obj["expectedUrl"];
      EXPECT_EQ(expected, *actual);
    }
  }
}
*/

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
