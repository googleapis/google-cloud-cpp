// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/default_client.h"
#include "google/cloud/storage/testing/mock_http_request.h"
#include <gmock/gmock.h>

namespace {
using storage::testing::MockHttpRequest;
using namespace ::testing;

class MockCredentials : public storage::Credentials {
 public:
  MOCK_METHOD0(AuthorizationHeader, std::string());
};

class DefaultClientTest : public ::testing::Test {
 protected:
  using DefaultClient = storage::internal::DefaultClient<MockHttpRequest>;

  void SetUp() {
    MockHttpRequest::Clear();
    credentials_ = std::make_shared<MockCredentials>();
    EXPECT_CALL(*credentials_, AuthorizationHeader())
        .WillRepeatedly(Return("some-secret-credential"));
  }
  void TearDown() {
    credentials_.reset();
    MockHttpRequest::Clear();
  }

  std::shared_ptr<MockCredentials> credentials_;
};

}  // anonymous namespace

TEST_F(DefaultClientTest, Simple) {
  auto handle = MockHttpRequest::Handle(
      "https://www.googleapis.com/storage/v1/b/my-bucket");
  EXPECT_CALL(*handle, PrepareRequest(An<std::string const&>()))
      .WillOnce(Invoke(
          [](std::string const& payload) { EXPECT_TRUE(payload.empty()); }));
  handle->SetupMakeEscapedString();
  EXPECT_CALL(*handle, AddHeader("Authorization", "some-secret-credential"))
      .Times(1);

  std::string response_payload = R"""({
      "kind": "storage#bucket",
      "id": "foo-bar-baz",
      "selfLink": "https://www.googleapis.com/storage/v1/b/foo-bar-baz",
      "projectNumber": "123456789",
      "name": "foo-bar-baz",
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z",
      "metageneration": "4",
      "location": "US",
      "storageClass": "STANDARD",
      "etag": "XYZ="
})""";
  EXPECT_CALL(*handle, MakeRequest())
      .WillOnce(
          Return(storage::internal::HttpResponse{200, response_payload, {}}));

  auto expected = storage::BucketMetadata::ParseFromJson(response_payload);

  DefaultClient client(credentials_);
  auto actual = client.GetBucketMetadata("my-bucket");
  EXPECT_TRUE(actual.first.ok());
  EXPECT_EQ(expected, actual.second);
}

TEST_F(DefaultClientTest, HandleError) {
  auto handle = MockHttpRequest::Handle(
      "https://www.googleapis.com/storage/v1/b/my-bucket");
  EXPECT_CALL(*handle, PrepareRequest(An<std::string const&>()))
      .WillOnce(Invoke(
          [](std::string const& payload) { EXPECT_TRUE(payload.empty()); }));
  EXPECT_CALL(*handle, AddHeader("Authorization", "some-secret-credential"))
      .Times(1);
  EXPECT_CALL(*handle, MakeRequest())
      .WillOnce(Return(
          storage::internal::HttpResponse{404, "cannot find my-bucket", {}}));

  DefaultClient client(credentials_);
  auto actual = client.GetBucketMetadata("my-bucket");
  EXPECT_FALSE(actual.first.ok());
  EXPECT_EQ(404, actual.first.status_code());
  EXPECT_EQ("cannot find my-bucket", actual.first.error_message());
}
