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

using ::google::cloud::storage::testing::TestPermanentFailure;

using ::testing::HasSubstr;

/// Store the file names captured from the command-line arguments.
class ObjectTestEnvironment : public ::testing::Environment {
 public:
  ObjectTestEnvironment(std::string account_file_name,
                        std::string data_file_name) {
    account_file_name_ = std::move(account_file_name);
    all_file_name_ = std::move(data_file_name);
  }

  static std::string const& account_file_name() { return account_file_name_; }
  static std::string const& data_file_name() { return all_file_name_; }

 private:
  static std::string account_file_name_;
  static std::string all_file_name_;
};

std::string ObjectTestEnvironment::account_file_name_;
std::string ObjectTestEnvironment::all_file_name_;

class ObjectIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {};

TEST_F(ObjectIntegrationTest, V4SignGet) {
  // This test uses a disabled key to create a V4 Signed URL for a GET
  // operation. The bucket name was generated at random too.

  // This is a dummy service account JSON file that is inactive. It's fine for
  // it to be public.
  std::string accountFile = ObjectTestEnvironment::account_file_name();
  std::string dataFile = ObjectTestEnvironment::data_file_name();

  std::ifstream ifs(accountFile);

  if (!ifs.is_open()) {
    // If the file does not exist, or
    // if we were unable to open it for some other reason.
    std::cout << "Cannot open credentials file " + accountFile + '\n';
    return;
  }
  nl::json jsonObj;
  jsonObj = nl::json::parse(ifs);

  // Turn json object into a string
  std::string file_contents = jsonObj.dump();
  std::string kJsonKeyfileContentsForV4 = file_contents;

  auto creds = oauth2::CreateServiceAccountCredentialsFromJsonContents(
      kJsonKeyfileContentsForV4);

  ASSERT_STATUS_OK(creds);
  Client client(*creds);

  std::ifstream ifstr(dataFile);

  if (!ifstr.is_open()) {
    // If the file does not exist, or
    // if we were unable to open it for some other reason.
    std::cout << "Cannot open credentials file " + dataFile + '\n';
    return;
  }

  nl::json jArray;
  nl::json jObj;
  jArray = nl::json::parse(ifstr);

  for (nl::json::iterator it = jArray.begin(); it != jArray.end(); ++it) {
    jObj = *it;
    if (jObj["method"] == "GET") {
      std::string const method_name = jObj["method"];  // GET
      std::string const bucket_name = jObj["bucket"];
      std::string const object_name = jObj["object"];
      std::string const date = jObj["timestamp"];
      int validInt = jObj["expiration"];
      auto const valid_for = std::chrono::seconds(validInt);

      auto actual = client.CreateV4SignedUrl(
          method_name, bucket_name, object_name,
          SignedUrlTimestamp(internal::ParseRfc3339(date)),
          SignedUrlDuration(valid_for),
          AddExtensionHeader("host", "storage.googleapis.com"));
      ASSERT_STATUS_OK(actual);

      EXPECT_THAT(*actual, HasSubstr(bucket_name));
      EXPECT_THAT(*actual, HasSubstr(object_name));

      std::string const expected = jObj["expectedUrl"];
      EXPECT_EQ(expected, *actual);
      return;
    }
  }
}

TEST_F(ObjectIntegrationTest, V4SignPut) {
  // This test uses a disabled key to create a V4 Signed URL for a GET
  // operation. The bucket name was generated at random too.

  // This is a dummy service account JSON file that is inactive. It's fine for
  // it to be public.
  std::string accountFile = ObjectTestEnvironment::account_file_name();
  std::string dataFile = ObjectTestEnvironment::data_file_name();

  std::ifstream ifs(accountFile);

  if (!ifs.is_open()) {
    // If the file does not exist, or
    // if we were unable to open it for some other reason.
    std::cout << "Cannot open credentials file " + accountFile + '\n';
    return;
  }
  nl::json jsonObj;
  jsonObj = nl::json::parse(ifs);

  // Turn json object into a string
  std::string file_contents = jsonObj.dump();
  std::string kJsonKeyfileContentsForV4 = file_contents;

  auto creds = oauth2::CreateServiceAccountCredentialsFromJsonContents(
      kJsonKeyfileContentsForV4);

  ASSERT_STATUS_OK(creds);
  Client client(*creds);

  std::ifstream ifstr(dataFile);

  if (!ifstr.is_open()) {
    // If the file does not exist, or
    // if we were unable to open it for some other reason.
    std::cout << "Cannot open credentials file " + dataFile + '\n';
    return;
  }

  nl::json jArray;
  nl::json jObj;
  jArray = nl::json::parse(ifstr);

  for (nl::json::iterator it = jArray.begin(); it != jArray.end(); ++it) {
    jObj = *it;
    if (jObj["method"] == "POST") {
      std::string const method_name = jObj["method"];  // POST
      std::string const bucket_name = jObj["bucket"];
      std::string const object_name = jObj["object"];
      std::string const date = jObj["timestamp"];

      std::string key_name;
      std::string header_name;
      for (auto& x : jObj["headers"].items()) {
        key_name = x.key();
        header_name = x.value()[0];
      }

      int validInt = jObj["expiration"];
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

      std::string const expected = jObj["expectedUrl"];
      EXPECT_EQ(expected, *actual);
      return;
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

  std::string const account_file_name = argv[1];
  std::string const data_file_name = argv[2];
  (void)::testing::AddGlobalTestEnvironment(
      new google::cloud::storage::ObjectTestEnvironment(account_file_name,
                                                        data_file_name));

  return RUN_ALL_TESTS();
}
