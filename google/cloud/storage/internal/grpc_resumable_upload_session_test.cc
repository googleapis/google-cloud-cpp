// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/grpc_resumable_upload_session.h"
#include "google/cloud/storage/internal/grpc_object_metadata_parser.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/testing/random_names.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::storage::testing::MakeRandomData;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AtLeast;
using ::testing::Return;

class MockInsertStream : public GrpcClient::WriteObjectStream {
 public:
  MOCK_METHOD(bool, Write,
              (google::storage::v2::WriteObjectRequest const&,
               grpc::WriteOptions),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::WriteObjectResponse>, Close, (),
              (override));
  MOCK_METHOD(void, Cancel, (), (override));
};

class MockGrpcClient : public GrpcClient {
 public:
  static std::shared_ptr<MockGrpcClient> Create() {
    return std::make_shared<MockGrpcClient>();
  }

  MockGrpcClient()
      : GrpcClient(DefaultOptionsGrpc(Options{}.set<GrpcCredentialOption>(
            grpc::InsecureChannelCredentials()))) {}

  MOCK_METHOD(std::unique_ptr<GrpcClient::WriteObjectStream>,
              CreateUploadWriter, (std::unique_ptr<grpc::ClientContext>),
              (override));
  MOCK_METHOD(StatusOr<ResumableUploadResponse>, QueryResumableUpload,
              (QueryResumableUploadRequest const&), (override));
};

StatusOr<google::storage::v2::WriteObjectResponse> MockCloseSuccess(
    std::uint64_t persisted_size) {
  google::storage::v2::WriteObjectResponse response;
  response.set_persisted_size(persisted_size);
  return response;
}

StatusOr<google::storage::v2::WriteObjectResponse> MockCloseError(Status s) {
  return StatusOr<google::storage::v2::WriteObjectResponse>(std::move(s));
}

TEST(GrpcResumableUploadSessionTest, Simple) {
  auto mock = MockGrpcClient::Create();
  ResumableUploadRequest request("test-bucket", "test-object");
  GrpcResumableUploadSession session(mock, request, {"test-upload-id"});

  std::string const payload = "test payload";
  auto const size = payload.size();
  HashValues hashes{ComputeCrc32cChecksum(payload), ComputeMD5Hash(payload)};
  auto const crc32c_proto =
      GrpcObjectMetadataParser::Crc32cToProto(hashes.crc32c);
  ASSERT_STATUS_OK(crc32c_proto);
  auto const md5_proto = GrpcObjectMetadataParser::MD5ToProto(hashes.md5);
  ASSERT_STATUS_OK(md5_proto);

  auto writer = absl::make_unique<MockInsertStream>();

  EXPECT_CALL(*mock, CreateUploadWriter)
      .WillOnce([&](std::unique_ptr<grpc::ClientContext>) {
        auto writer = absl::make_unique<MockInsertStream>();
        EXPECT_CALL(*writer, Write)
            .WillOnce([&](google::storage::v2::WriteObjectRequest const& r,
                          grpc::WriteOptions const& options) {
              EXPECT_EQ("test-upload-id", r.upload_id());
              EXPECT_EQ(payload, r.checksummed_data().content());
              EXPECT_EQ(0, r.write_offset());
              EXPECT_FALSE(r.finish_write());
              EXPECT_FALSE(options.is_last_message());
              return true;
            });
        EXPECT_CALL(*writer, Close()).WillOnce(Return(MockCloseSuccess(size)));
        return std::unique_ptr<GrpcClient::WriteObjectStream>(writer.release());
      })
      .WillOnce([&](std::unique_ptr<grpc::ClientContext>) {
        auto writer = absl::make_unique<MockInsertStream>();
        EXPECT_CALL(*writer, Write)
            .WillOnce([&](google::storage::v2::WriteObjectRequest const& r,
                          grpc::WriteOptions const& options) {
              EXPECT_EQ("test-upload-id", r.upload_id());
              EXPECT_EQ(payload, r.checksummed_data().content());
              EXPECT_EQ(size, r.write_offset());
              EXPECT_TRUE(r.finish_write());
              EXPECT_EQ(*crc32c_proto, r.object_checksums().crc32c());
              EXPECT_EQ(*md5_proto, r.object_checksums().md5_hash());
              EXPECT_TRUE(options.is_last_message());
              return true;
            });
        EXPECT_CALL(*writer, Close())
            .WillOnce(Return(MockCloseSuccess(2 * size)));
        return std::unique_ptr<GrpcClient::WriteObjectStream>(writer.release());
      });

  auto upload = session.UploadChunk({{payload}});
  ASSERT_STATUS_OK(upload);
  EXPECT_EQ(size, upload->committed_size.value_or(0));

  upload = session.UploadFinalChunk({{payload}}, 2 * size, hashes);
  EXPECT_STATUS_OK(upload);
  EXPECT_EQ(2 * size, upload->committed_size.value_or(0));
}

TEST(GrpcResumableUploadSessionTest, SingleStreamForLargeChunks) {
  auto mock = MockGrpcClient::Create();
  ResumableUploadRequest request("test-bucket", "test-object");
  GrpcResumableUploadSession session(mock, request, {"test-upload-id"});

  auto rng = google::cloud::internal::MakeDefaultPRNG();
  auto const payload = MakeRandomData(rng, 8 * 1024 * 1024L);
  auto const size = payload.size();

  std::size_t expected_write_offset = 0;
  EXPECT_CALL(*mock, CreateUploadWriter)
      .Times(AtLeast(1))
      .WillRepeatedly([&](std::unique_ptr<grpc::ClientContext>) {
        auto writer = absl::make_unique<MockInsertStream>();

        using ::google::storage::v2::WriteObjectRequest;
        EXPECT_CALL(*writer, Write)
            .Times(AtLeast(2))
            .WillRepeatedly([&](WriteObjectRequest const& r,
                                grpc::WriteOptions const&) {
              EXPECT_EQ("test-upload-id", r.upload_id());
              EXPECT_EQ(expected_write_offset, r.write_offset());
              EXPECT_TRUE(r.has_checksummed_data());
              auto const content_size = r.checksummed_data().content().size();
              EXPECT_LE(
                  content_size,
                  google::storage::v2::ServiceConstants::MAX_WRITE_CHUNK_BYTES);
              expected_write_offset += content_size;
              return true;
            });
        EXPECT_CALL(*writer, Close).WillOnce([&] {
          return MockCloseSuccess(expected_write_offset);
        });

        return std::unique_ptr<GrpcClient::WriteObjectStream>(writer.release());
      });

  auto upload = session.UploadChunk({{payload}});
  EXPECT_STATUS_OK(upload);
  EXPECT_EQ(size, upload->committed_size.value_or(0));

  upload = session.UploadFinalChunk({{payload}}, 2 * size, HashValues{});
  EXPECT_STATUS_OK(upload);
  EXPECT_EQ(2 * size, upload->committed_size.value_or(0));
}

TEST(GrpcResumableUploadSessionTest, Reset) {
  auto mock = MockGrpcClient::Create();
  ResumableUploadRequest request("test-bucket", "test-object");
  GrpcResumableUploadSession session(mock, request, {"test-upload-id"});

  std::string const payload = "test payload";
  auto const size = payload.size();

  EXPECT_CALL(*mock, CreateUploadWriter)
      .WillOnce([&](std::unique_ptr<grpc::ClientContext>) {
        auto writer = absl::make_unique<MockInsertStream>();
        EXPECT_CALL(*writer, Write)
            .WillOnce([&](google::storage::v2::WriteObjectRequest const& r,
                          grpc::WriteOptions const&) {
              EXPECT_EQ("test-upload-id", r.upload_id());
              EXPECT_EQ(payload, r.checksummed_data().content());
              EXPECT_EQ(0, r.write_offset());
              EXPECT_FALSE(r.finish_write());
              return true;
            });
        EXPECT_CALL(*writer, Close)
            .WillOnce(Return(
                MockCloseError(Status(StatusCode::kUnavailable, "try again"))));
        return std::unique_ptr<GrpcClient::WriteObjectStream>(writer.release());
      })
      .WillOnce([&](std::unique_ptr<grpc::ClientContext>) {
        auto writer = absl::make_unique<MockInsertStream>();
        EXPECT_CALL(*writer, Write)
            .WillOnce([&](google::storage::v2::WriteObjectRequest const& r,
                          grpc::WriteOptions const&) {
              EXPECT_EQ("test-upload-id", r.upload_id());
              EXPECT_EQ(payload, r.checksummed_data().content());
              EXPECT_EQ(size, r.write_offset());
              EXPECT_FALSE(r.finish_write());
              return true;
            });
        EXPECT_CALL(*writer, Close)
            .WillOnce(Return(MockCloseSuccess(2 * size)));
        return std::unique_ptr<GrpcClient::WriteObjectStream>(writer.release());
      });

  EXPECT_CALL(*mock, QueryResumableUpload)
      .WillOnce([&](QueryResumableUploadRequest const& request) {
        EXPECT_EQ("test-upload-id", request.upload_session_url());
        return make_status_or(ResumableUploadResponse{
            {}, ResumableUploadResponse::kInProgress, size, {}, {}});
      });

  auto upload = session.UploadChunk({{payload}});
  EXPECT_THAT(upload, StatusIs(StatusCode::kUnavailable));

  upload = session.ResetSession();
  EXPECT_STATUS_OK(upload);
  EXPECT_EQ(size, upload->committed_size.value_or(0));

  upload = session.UploadChunk({{payload}});
  EXPECT_STATUS_OK(upload);
  EXPECT_EQ(2 * size, upload->committed_size.value_or(0));
  auto decoded_session_url =
      DecodeGrpcResumableUploadSessionUrl(session.session_id());
  ASSERT_STATUS_OK(decoded_session_url);
  EXPECT_EQ("test-upload-id", decoded_session_url->upload_id);
}

TEST(GrpcResumableUploadSessionTest, ResumeFromEmpty) {
  auto mock = MockGrpcClient::Create();
  ResumableUploadRequest request("test-bucket", "test-object");
  GrpcResumableUploadSession session(mock, request, {"test-upload-id"});

  std::string const payload = "test payload";
  auto const size = payload.size();

  EXPECT_CALL(*mock, CreateUploadWriter)
      .WillOnce([&](std::unique_ptr<grpc::ClientContext>) {
        auto writer = absl::make_unique<MockInsertStream>();

        EXPECT_CALL(*writer, Write)
            .WillOnce([&](google::storage::v2::WriteObjectRequest const& r,
                          grpc::WriteOptions const&) {
              EXPECT_EQ("test-upload-id", r.upload_id());
              EXPECT_EQ(payload, r.checksummed_data().content());
              EXPECT_EQ(0, r.write_offset());
              EXPECT_TRUE(r.finish_write());
              return false;
            });
        EXPECT_CALL(*writer, Close)
            .WillOnce(Return(
                MockCloseError(Status(StatusCode::kUnavailable, "try again"))));

        return std::unique_ptr<GrpcClient::WriteObjectStream>(writer.release());
      })
      .WillOnce([&](std::unique_ptr<grpc::ClientContext>) {
        auto writer = absl::make_unique<MockInsertStream>();

        EXPECT_CALL(*writer, Write)
            .WillOnce([&](google::storage::v2::WriteObjectRequest const& r,
                          grpc::WriteOptions const&) {
              EXPECT_EQ("test-upload-id", r.upload_id());
              EXPECT_EQ(payload, r.checksummed_data().content());
              EXPECT_EQ(0, r.write_offset());
              EXPECT_TRUE(r.finish_write());
              return true;
            });
        EXPECT_CALL(*writer, Close).WillOnce(Return(MockCloseSuccess(size)));

        return std::unique_ptr<GrpcClient::WriteObjectStream>(writer.release());
      });

  ResumableUploadResponse const resume_response{
      {}, ResumableUploadResponse::kInProgress, 0, {}, {}};
  EXPECT_CALL(*mock, QueryResumableUpload)
      .WillOnce([&](QueryResumableUploadRequest const& request) {
        EXPECT_EQ("test-upload-id", request.upload_session_url());
        return make_status_or(resume_response);
      });

  auto upload = session.UploadFinalChunk({{payload}}, size, HashValues{});
  EXPECT_THAT(upload, StatusIs(StatusCode::kUnavailable));

  auto response = session.ResetSession();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(0, response->committed_size.value_or(0));

  auto decoded_session_url =
      DecodeGrpcResumableUploadSessionUrl(session.session_id());
  ASSERT_STATUS_OK(decoded_session_url);
  EXPECT_EQ("test-upload-id", decoded_session_url->upload_id);

  upload = session.UploadFinalChunk({{payload}}, size, HashValues{});
  ASSERT_STATUS_OK(upload);
  EXPECT_EQ(size, upload->committed_size.value_or(0));
  EXPECT_EQ(ResumableUploadResponse::kInProgress, upload->upload_state);
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
