#include "google/cloud/storage/client.h"
#include <gmock/gmock.h>
#include <iostream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {

int main() {
  ClientOptions client_options =
      ClientOptions(oauth2::CreateAnonymousCredentials());

  std::shared_ptr<::testing::MockClient> mock =
      std::make_shared<::testing::MockClient>();
  EXPECT_CALL(*mock, client_options())
      .WillRepeatedly(::testing::ReturnRef(client_options));

  Client client(mock);

  ObjectMetadata expected_metadata;

  EXPECT_CALL(*mock, CreateResumableSession(::testing::_))
      .WillOnce([&expected_metadata](
                    internal::ResumableUploadRequest const& request) {
        EXPECT_EQ(request.bucket_name(), "mock-bucket-name") << request;
        auto* mock_result = new ::testing::MockResumableUploadSession;
        using internal::ResumableUploadResponse;
        EXPECT_CALL(*mock_result, done())
            .WillRepeatedly(::testing::Return(false));
        EXPECT_CALL(*mock_result, next_expected_byte())
            .WillRepeatedly(::testing::Return(0));
        EXPECT_CALL(*mock_result, UploadChunk(::testing::_))
            .WillRepeatedly(::testing::Return(google::cloud::make_status_or(
                ResumableUploadResponse{"fake-url",
                                        0,
                                        {},
                                        ResumableUploadResponse::kInProgress,
                                        {}})));
        EXPECT_CALL(*mock_result, UploadFinalChunk(::testing::_, ::testing::_))
            .WillRepeatedly(::testing::Return(google::cloud::make_status_or(
                ResumableUploadResponse{"fake-url",
                                        0,
                                        expected_metadata,
                                        ResumableUploadResponse::kDone,
                                        {}})));

        std::unique_ptr<internal::ResumableUploadSession> result(mock_result);
        return google::cloud::make_status_or(std::move(result));
      });

  auto stream = client.WriteObject("mock-bucket-name", "mock-object-name");
  stream << "Bring me to work!";

  const auto& session_url = stream.resumable_session_id();
  const auto& client_status =
      client.DeleteResumableUpload(session_url, client_options);

  stream.Close();

  return 0;
}
}  // namespace STORAGE_CLIENT_NS

}  // namespace storage
}  // namespace cloud
}  // namespace google