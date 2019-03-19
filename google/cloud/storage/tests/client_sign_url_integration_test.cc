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

TEST_F(ObjectIntegrationTest, V4SignGet) {
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

  nl::json json_array;
  json_array = nl::json::parse(ifstr);

  for (auto const& j_obj : json_array) {
    if (j_obj["method"] == "GET" && j_obj["description"] == "Simple GET") {
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

TEST_F(ObjectIntegrationTest, V4SignPost) {
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

  nl::json json_array;
  json_array = nl::json::parse(ifstr);

  for (auto const& j_obj : json_array) {
    if (j_obj["method"] == "POST") {
      std::string const method_name = j_obj["method"];  // POST
      std::string const bucket_name = j_obj["bucket"];
      std::string const object_name = j_obj["object"];
      std::string const date = j_obj["timestamp"];

      std::string key_name;
      std::string header_name;
      for (auto& x : j_obj["headers"].items()) {
        key_name = x.key();
        header_name = x.value()[0];
      }

      int validInt = j_obj["expiration"];
      auto const valid_for = std::chrono::seconds(validInt);

      auto actual = client.CreateV4SignedUrl(
          method_name, bucket_name, object_name,
          SignedUrlTimestamp(internal::ParseRfc3339(date)),
          SignedUrlDuration(valid_for),
          AddExtensionHeader("host", "storage.googleapis.com"),
          AddExtensionHeader(key_name, header_name));

      ASSERT_STATUS_OK(actual);
      EXPECT_THAT(*actual, HasSubstr(bucket_name));
      EXPECT_THAT(*actual, HasSubstr(object_name));

      std::string const expected = j_obj["expectedUrl"];
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
