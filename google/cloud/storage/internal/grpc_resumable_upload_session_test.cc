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

#include "google/cloud/storage/internal/grpc_resumable_upload_session.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::testing::_;
using ::testing::Return;

class MockGrpcUploadWriter : public GrpcClient::UploadWriter {
 public:
  MOCK_METHOD2(Write, bool(google::storage::v1::InsertObjectRequest const&,
                           grpc::WriteOptions));
  MOCK_METHOD0(WritesDone, bool());
  MOCK_METHOD0(Finish, grpc::Status());
};

class MockGrpcClient : public GrpcClient {
 public:
  static std::shared_ptr<MockGrpcClient> Create() {
    return std::make_shared<MockGrpcClient>();
  }

  MockGrpcClient()
      : GrpcClient(ClientOptions(oauth2::CreateAnonymousCredentials())) {}

  MOCK_METHOD2(CreateUploadWriter,
               std::unique_ptr<GrpcClient::UploadWriter>(
                   grpc::ClientContext&, google::storage::v1::Object&));
  MOCK_METHOD1(QueryResumableUpload, StatusOr<ResumableUploadResponse>(
                                         QueryResumableUploadRequest const&));
};

TEST(GrpcResumableUploadSessionTest, Simple) {
  auto mock = MockGrpcClient::Create();
  GrpcResumableUploadSession session(mock, "test-upload-id");

  std::string const payload = "test payload";
  auto const size = payload.size();

  EXPECT_FALSE(session.done());
  EXPECT_EQ(0, session.next_expected_byte());
  EXPECT_CALL(*mock, CreateUploadWriter(_, _))
      .WillOnce([&](grpc::ClientContext&, google::storage::v1::Object&) {
        auto writer = absl::make_unique<MockGrpcUploadWriter>();

        EXPECT_CALL(*writer, Write(_, _))
            .WillOnce([&](google::storage::v1::InsertObjectRequest const& r,
                          grpc::WriteOptions const& options) {
              EXPECT_EQ("test-upload-id", r.upload_id());
              EXPECT_EQ(payload, r.checksummed_data().content());
              EXPECT_EQ(0, r.write_offset());
              EXPECT_FALSE(r.finish_write());
              EXPECT_FALSE(options.is_last_message());
              return true;
            })
            .WillOnce([&](google::storage::v1::InsertObjectRequest const& r,
                          grpc::WriteOptions const& options) {
              EXPECT_EQ("test-upload-id", r.upload_id());
              EXPECT_EQ(payload, r.checksummed_data().content());
              EXPECT_EQ(size, r.write_offset());
              EXPECT_TRUE(r.finish_write());
              EXPECT_TRUE(options.is_last_message());
              return true;
            });
        EXPECT_CALL(*writer, Finish()).WillOnce(Return(grpc::Status::OK));

        return std::unique_ptr<GrpcClient::UploadWriter>(writer.release());
      });

  auto upload = session.UploadChunk({{payload}});
  EXPECT_STATUS_OK(upload);
  EXPECT_EQ(size - 1, upload->last_committed_byte);
  EXPECT_EQ(size, session.next_expected_byte());
  EXPECT_FALSE(session.done());

  upload = session.UploadFinalChunk({{payload}}, 2 * size);
  EXPECT_STATUS_OK(upload);
  EXPECT_EQ(2 * size - 1, upload->last_committed_byte);
  EXPECT_EQ(2 * size, session.next_expected_byte());
  EXPECT_TRUE(session.done());
}

TEST(GrpcResumableUploadSessionTest, Reset) {
  auto mock = MockGrpcClient::Create();
  GrpcResumableUploadSession session(mock, "test-upload-id");

  std::string const payload = "test payload";
  auto const size = payload.size();

  EXPECT_EQ(0, session.next_expected_byte());
  EXPECT_CALL(*mock, CreateUploadWriter(_, _))
      .WillOnce([&](grpc::ClientContext&, google::storage::v1::Object&) {
        auto writer = absl::make_unique<MockGrpcUploadWriter>();

        EXPECT_CALL(*writer, Write(_, _))
            .WillOnce([&](google::storage::v1::InsertObjectRequest const& r,
                          grpc::WriteOptions const&) {
              EXPECT_EQ("test-upload-id", r.upload_id());
              EXPECT_EQ(payload, r.checksummed_data().content());
              EXPECT_EQ(0, r.write_offset());
              EXPECT_FALSE(r.finish_write());
              return true;
            })
            .WillOnce([&](google::storage::v1::InsertObjectRequest const& r,
                          grpc::WriteOptions const&) {
              EXPECT_EQ("test-upload-id", r.upload_id());
              EXPECT_EQ(payload, r.checksummed_data().content());
              EXPECT_EQ(size, r.write_offset());
              EXPECT_FALSE(r.finish_write());
              return false;
            });
        EXPECT_CALL(*writer, Finish())
            .WillOnce(Return(
                grpc::Status(grpc::StatusCode::UNAVAILABLE, "try again")));

        return std::unique_ptr<GrpcClient::UploadWriter>(writer.release());
      });

  ResumableUploadResponse const resume_response{
      {}, 2 * size - 1, {}, ResumableUploadResponse::kInProgress, {}};
  EXPECT_CALL(*mock, QueryResumableUpload(_))
      .WillOnce([&](QueryResumableUploadRequest const& request) {
        EXPECT_EQ("test-upload-id", request.upload_session_url());
        return make_status_or(resume_response);
      });

  auto upload = session.UploadChunk({{payload}});
  EXPECT_EQ(size, session.next_expected_byte());
  EXPECT_STATUS_OK(upload);
  upload = session.UploadChunk({{payload}});
  EXPECT_EQ(StatusCode::kUnavailable, upload.status().code());

  session.ResetSession();
  EXPECT_EQ(2 * size, session.next_expected_byte());
  EXPECT_EQ("test-upload-id", session.session_id());
  StatusOr<ResumableUploadResponse> const& last_response =
      session.last_response();
  ASSERT_STATUS_OK(last_response);
  EXPECT_EQ(last_response.value(), resume_response);
}

TEST(GrpcResumableUploadSessionTest, ResumeFromEmpty) {
  auto mock = MockGrpcClient::Create();
  GrpcResumableUploadSession session(mock, "test-upload-id");

  std::string const payload = "test payload";
  auto const size = payload.size();

  EXPECT_EQ(0, session.next_expected_byte());
  EXPECT_CALL(*mock, CreateUploadWriter(_, _))
      .WillOnce([&](grpc::ClientContext&, google::storage::v1::Object&) {
        auto writer = absl::make_unique<MockGrpcUploadWriter>();

        EXPECT_CALL(*writer, Write(_, _))
            .WillOnce([&](google::storage::v1::InsertObjectRequest const& r,
                          grpc::WriteOptions const&) {
              EXPECT_EQ("test-upload-id", r.upload_id());
              EXPECT_EQ(payload, r.checksummed_data().content());
              EXPECT_EQ(0, r.write_offset());
              EXPECT_TRUE(r.finish_write());
              return false;
            });
        EXPECT_CALL(*writer, Finish())
            .WillOnce(Return(
                grpc::Status(grpc::StatusCode::UNAVAILABLE, "try again")));

        return std::unique_ptr<GrpcClient::UploadWriter>(writer.release());
      })
      .WillOnce([&](grpc::ClientContext&, google::storage::v1::Object&) {
        auto writer = absl::make_unique<MockGrpcUploadWriter>();

        EXPECT_CALL(*writer, Write(_, _))
            .WillOnce([&](google::storage::v1::InsertObjectRequest const& r,
                          grpc::WriteOptions const&) {
              EXPECT_EQ("test-upload-id", r.upload_id());
              EXPECT_EQ(payload, r.checksummed_data().content());
              EXPECT_EQ(0, r.write_offset());
              EXPECT_TRUE(r.finish_write());
              return true;
            });
        EXPECT_CALL(*writer, Finish()).WillOnce(Return(grpc::Status::OK));

        return std::unique_ptr<GrpcClient::UploadWriter>(writer.release());
      });

  ResumableUploadResponse const resume_response{
      {}, 0, {}, ResumableUploadResponse::kInProgress, {}};
  EXPECT_CALL(*mock, QueryResumableUpload(_))
      .WillOnce([&](QueryResumableUploadRequest const& request) {
        EXPECT_EQ("test-upload-id", request.upload_session_url());
        return make_status_or(resume_response);
      });

  auto upload = session.UploadFinalChunk({{payload}}, size);
  EXPECT_EQ(StatusCode::kUnavailable, upload.status().code());

  session.ResetSession();
  EXPECT_EQ(0, session.next_expected_byte());
  EXPECT_EQ("test-upload-id", session.session_id());
  StatusOr<ResumableUploadResponse> const& last_response =
      session.last_response();
  ASSERT_STATUS_OK(last_response);
  EXPECT_EQ(last_response.value(), resume_response);

  upload = session.UploadFinalChunk({{payload}}, size);
  EXPECT_STATUS_OK(upload);
  EXPECT_EQ(size, session.next_expected_byte());
  EXPECT_TRUE(session.done());
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
