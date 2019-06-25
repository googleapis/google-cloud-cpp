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

#include "google/cloud/storage/internal/curl_resumable_upload_session.h"
#include "google/cloud/storage/internal/curl_client.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::testing::_;
using ::testing::Invoke;

class MockCurlClient : public CurlClient {
 public:
  static std::shared_ptr<MockCurlClient> Create() {
    return std::make_shared<MockCurlClient>();
  }

  MockCurlClient()
      : CurlClient(ClientOptions(oauth2::CreateAnonymousCredentials())) {}

  MOCK_METHOD1(UploadChunk,
               StatusOr<ResumableUploadResponse>(UploadChunkRequest const&));
  MOCK_METHOD1(QueryResumableUpload, StatusOr<ResumableUploadResponse>(
                                         QueryResumableUploadRequest const&));
};

TEST(CurlResumableUploadSessionTest, Simple) {
  auto mock = MockCurlClient::Create();
  std::string test_url = "http://invalid.example.com/not-used-in-mock";
  CurlResumableUploadSession session(mock, test_url);

  std::string const payload = "test payload";
  auto const size = payload.size();

  EXPECT_FALSE(session.done());
  EXPECT_EQ(0, session.next_expected_byte());
  EXPECT_CALL(*mock, UploadChunk(_))
      .WillOnce(Invoke([&](UploadChunkRequest const& request) {
        EXPECT_EQ(test_url, request.upload_session_url());
        EXPECT_EQ(payload, request.payload());
        EXPECT_EQ(0, request.source_size());
        EXPECT_EQ(0, request.range_begin());
        return make_status_or(ResumableUploadResponse{
            "", size - 1, "", ResumableUploadResponse::kInProgress});
      }))
      .WillOnce(Invoke([&](UploadChunkRequest const& request) {
        EXPECT_EQ(test_url, request.upload_session_url());
        EXPECT_EQ(payload, request.payload());
        EXPECT_EQ(2 * size, request.source_size());
        EXPECT_EQ(size, request.range_begin());
        return make_status_or(ResumableUploadResponse{
            "", 2 * size - 1, "", ResumableUploadResponse::kDone});
      }));

  auto upload = session.UploadChunk(payload);
  EXPECT_STATUS_OK(upload);
  EXPECT_EQ(size - 1, upload->last_committed_byte);
  EXPECT_EQ(size, session.next_expected_byte());
  EXPECT_FALSE(session.done());

  upload = session.UploadFinalChunk(payload, 2 * size);
  EXPECT_STATUS_OK(upload);
  EXPECT_EQ(2 * size - 1, upload->last_committed_byte);
  EXPECT_EQ(2 * size, session.next_expected_byte());
  EXPECT_TRUE(session.done());
}

TEST(CurlResumableUploadSessionTest, Reset) {
  auto mock = MockCurlClient::Create();
  std::string url1 = "http://invalid.example.com/not-used-in-mock-1";
  std::string url2 = "http://invalid.example.com/not-used-in-mock-2";
  CurlResumableUploadSession session(mock, url1);

  std::string const payload = "test payload";
  auto const size = payload.size();

  EXPECT_EQ(0, session.next_expected_byte());
  EXPECT_CALL(*mock, UploadChunk(_))
      .WillOnce(Invoke([&](UploadChunkRequest const&) {
        return make_status_or(ResumableUploadResponse{
            "", size - 1, "", ResumableUploadResponse::kInProgress});
      }))
      .WillOnce(Invoke([&](UploadChunkRequest const&) {
        return StatusOr<ResumableUploadResponse>(
            AsStatus(HttpResponse{308, "uh oh", {}}));
      }));
  const ResumableUploadResponse resume_response{
      url2, 2 * size - 1, "", ResumableUploadResponse::kInProgress};
  EXPECT_CALL(*mock, QueryResumableUpload(_))
      .WillOnce(Invoke([&](QueryResumableUploadRequest const& request) {
        EXPECT_EQ(url1, request.upload_session_url());
        return make_status_or(resume_response);
      }));

  auto upload = session.UploadChunk(payload);
  EXPECT_EQ(size, session.next_expected_byte());
  upload = session.UploadChunk(payload);
  EXPECT_FALSE(upload.ok());
  EXPECT_EQ(size, session.next_expected_byte());
  EXPECT_EQ(url1, session.session_id());

  session.ResetSession();
  EXPECT_EQ(2 * size, session.next_expected_byte());
  EXPECT_EQ(url2, session.session_id());
  StatusOr<ResumableUploadResponse> const& last_response =
      session.last_response();
  ASSERT_STATUS_OK(last_response);
  EXPECT_EQ(last_response.value(), resume_response);
}

TEST(CurlResumableUploadSessionTest, SessionUpdatedInChunkUpload) {
  auto mock = MockCurlClient::Create();
  std::string url1 = "http://invalid.example.com/not-used-in-mock-1";
  std::string url2 = "http://invalid.example.com/not-used-in-mock-2";
  CurlResumableUploadSession session(mock, url1);

  std::string const payload = "test payload";
  auto const size = payload.size();

  EXPECT_EQ(0, session.next_expected_byte());
  EXPECT_CALL(*mock, UploadChunk(_))
      .WillOnce(Invoke([&](UploadChunkRequest const&) {
        return make_status_or(ResumableUploadResponse{
            "", size - 1, "", ResumableUploadResponse::kInProgress});
      }))
      .WillOnce(Invoke([&](UploadChunkRequest const&) {
        return make_status_or(ResumableUploadResponse{
            url2, 2 * size - 1, "", ResumableUploadResponse::kInProgress});
      }));

  auto upload = session.UploadChunk(payload);
  EXPECT_EQ(size, session.next_expected_byte());
  upload = session.UploadChunk(payload);
  EXPECT_STATUS_OK(upload);
  EXPECT_EQ(2 * size, session.next_expected_byte());
  EXPECT_EQ(url2, session.session_id());
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
