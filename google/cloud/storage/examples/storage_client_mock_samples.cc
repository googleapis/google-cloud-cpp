// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/examples/storage_examples_common.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/storage/well_known_parameters.h"
#include "google/cloud/internal/make_unique.h"
#include <gmock/gmock.h>
#include <functional>
#include <iostream>
#include <string>

namespace {

using google::cloud::storage::examples::Usage;

void MockReadObject(std::vector<std::string> argv) {
  if (argv.size() != 3) {
    throw Usage{"mock-read-object <bucket-name> <object-name>"};
  }
  auto bucket_name = argv[0];
  auto object_name = argv[1];

  //! [mock successful readobject]
  namespace gcs = google::cloud::storage;
  gcs::ClientOptions client_options =
      gcs::ClientOptions(gcs::oauth2::CreateAnonymousCredentials());

  std::shared_ptr<gcs::testing::MockClient> mock =
      std::make_shared<gcs::testing::MockClient>();
  EXPECT_CALL(*mock, client_options())
      .WillRepeatedly(testing::ReturnRef(client_options));

  gcs::Client client(mock);

  std::string text = "this is a mock http response";
  using ::testing::_;
  EXPECT_CALL(*mock, ReadObject(_))
      .WillOnce(testing::Invoke(
          [&text](gcs::internal::ReadObjectRangeRequest const& request) {
            std::cout << "\nReading from bucket : " << request.bucket_name()
                      << std::endl;
            auto* mock_source = new gcs::testing::MockObjectReadSource;
            EXPECT_CALL(*mock_source, IsOpen())
                .WillOnce(testing::Return(true))
                .WillRepeatedly(testing::Return(false));
            EXPECT_CALL(*mock_source, Read(_, _))
                .WillOnce(testing::Return(gcs::internal::ReadSourceResult{
                    text.length(),
                    gcs::internal::HttpResponse{200, text, {}}}));

            std::unique_ptr<gcs::internal::ObjectReadSource> result(
                mock_source);

            return google::cloud::make_status_or(std::move(result));
          }));

  gcs::ObjectReadStream stream = client.ReadObject(bucket_name, object_name);

  // Reading the payload of http response stored in stream
  int count = 0;
  std::string line;
  while (std::getline(stream, line, '\n')) {
    std::cout << "Reading line : " << line << std::endl;
    ++count;
  }

  std::cout << "The object has " << count << " line(s)\n";
  stream.Close();
  //! [mock successful readobject]
}

void MockWriteObject(std::vector<std::string> argv) {
  if (argv.size() != 3) {
    throw Usage{"mock-write-object <bucket-name> <object-name>"};
  }
  auto bucket_name = argv[0];
  auto object_name = argv[1];

  //! [mock successful writeobject]
  namespace gcs = google::cloud::storage;
  gcs::ClientOptions client_options =
      gcs::ClientOptions(gcs::oauth2::CreateAnonymousCredentials());

  std::shared_ptr<gcs::testing::MockClient> mock =
      std::make_shared<gcs::testing::MockClient>();
  EXPECT_CALL(*mock, client_options())
      .WillRepeatedly(testing::ReturnRef(client_options));

  gcs::Client client(mock);

  gcs::ObjectMetadata expected_metadata;

  using ::testing::_;
  using ::testing::Return;
  EXPECT_CALL(*mock, CreateResumableSession(_))
      .WillOnce(testing::Invoke(
          [&expected_metadata](
              gcs::internal::ResumableUploadRequest const& request) {
            std::cout << "Writing to bucket : " << request.bucket_name()
                      << std::endl;
            auto* mock_result = new gcs::testing::MockResumableUploadSession;
            using gcs::internal::ResumableUploadResponse;
            EXPECT_CALL(*mock_result, done()).WillRepeatedly(Return(false));
            EXPECT_CALL(*mock_result, next_expected_byte())
                .WillRepeatedly(Return(0));
            EXPECT_CALL(*mock_result, UploadChunk(_))
                .WillRepeatedly(Return(
                    google::cloud::make_status_or(ResumableUploadResponse{
                        "fake-url",
                        0,
                        {},
                        ResumableUploadResponse::kInProgress,
                        {}})));
            EXPECT_CALL(*mock_result, UploadFinalChunk(_, _))
                .WillRepeatedly(Return(google::cloud::make_status_or(
                    ResumableUploadResponse{"fake-url",
                                            0,
                                            expected_metadata,
                                            ResumableUploadResponse::kDone,
                                            {}})));

            std::unique_ptr<gcs::internal::ResumableUploadSession> result(
                mock_result);
            return google::cloud::make_status_or(std::move(result));
          }));

  auto stream = client.WriteObject(bucket_name, object_name);
  stream << "Hello World!";
  stream.Close();
  //! [mock successful writeobject]
}

void MockReadObjectFailure(std::vector<std::string> argv) {
  if (argv.size() != 3) {
    throw Usage{"mock-read-object-failure <bucket-name> <object-name>"};
  }
  auto bucket_name = argv[0];
  auto object_name = argv[1];

  //! [mock failed readobject]
  namespace gcs = google::cloud::storage;
  gcs::ClientOptions client_options =
      gcs::ClientOptions(gcs::oauth2::CreateAnonymousCredentials());

  std::shared_ptr<gcs::testing::MockClient> mock =
      std::make_shared<gcs::testing::MockClient>();
  EXPECT_CALL(*mock, client_options())
      .WillRepeatedly(testing::ReturnRef(client_options));

  gcs::Client client(mock);

  std::string text = "this is a mock http response";
  using ::testing::_;
  EXPECT_CALL(*mock, ReadObject(_))
      .WillOnce(testing::Invoke(
          [](gcs::internal::ReadObjectRangeRequest const& request) {
            std::cout << "\nReading from bucket : " << request.bucket_name()
                      << std::endl;
            auto* mock_source = new gcs::testing::MockObjectReadSource;
            EXPECT_CALL(*mock_source, IsOpen())
                .WillOnce(testing::Return(true))
                .WillRepeatedly(testing::Return(false));
            EXPECT_CALL(*mock_source, Read(_, _))
                .WillOnce(testing::Return(google::cloud::Status(
                    google::cloud::StatusCode::kInvalidArgument,
                    "Invalid Argument")));

            std::unique_ptr<gcs::internal::ObjectReadSource> result(
                mock_source);

            return google::cloud::make_status_or(std::move(result));
          }))
      .WillOnce(testing::Invoke(
          [](gcs::internal::ReadObjectRangeRequest const& request) {
            std::cout << "\nRetry reading from bucket : "
                      << request.bucket_name() << std::endl;
            auto* mock_source = new gcs::testing::MockObjectReadSource;
            EXPECT_CALL(*mock_source, Read(_, _))
                .WillOnce(testing::Return(google::cloud::Status(
                    google::cloud::StatusCode::kInvalidArgument,
                    "Invalid Argument")));

            std::unique_ptr<gcs::internal::ObjectReadSource> result(
                mock_source);

            return google::cloud::make_status_or(std::move(result));
          }));

  gcs::ObjectReadStream stream = client.ReadObject(bucket_name, object_name);

  if (stream.bad()) {
    std::cerr << "\nError reading from stream... status=" << stream.status()
              << std::endl;
  }
  stream.Close();

  //! [mock failed readobject]
}

void MockWriteObjectFailure(std::vector<std::string> argv) {
  if (argv.size() != 3) {
    throw Usage{"mock-read-object-failure <bucket-name> <object-name>"};
  }
  auto bucket_name = argv[0];
  auto object_name = argv[1];

  //! [mock failed writeobject]
  namespace gcs = google::cloud::storage;
  gcs::ClientOptions client_options =
      gcs::ClientOptions(gcs::oauth2::CreateAnonymousCredentials());

  std::shared_ptr<gcs::testing::MockClient> mock =
      std::make_shared<gcs::testing::MockClient>();
  EXPECT_CALL(*mock, client_options())
      .WillRepeatedly(testing::ReturnRef(client_options));

  gcs::Client client(mock);

  using ::testing::_;
  using ::testing::Return;
  EXPECT_CALL(*mock, CreateResumableSession(_))
      .WillOnce(testing::Invoke(
          [](gcs::internal::ResumableUploadRequest const& request) {
            std::cout << "Writing to bucket : " << request.bucket_name()
                      << std::endl;
            auto* mock_result = new gcs::testing::MockResumableUploadSession;
            using gcs::internal::ResumableUploadResponse;
            EXPECT_CALL(*mock_result, done()).WillRepeatedly(Return(false));
            EXPECT_CALL(*mock_result, next_expected_byte())
                .WillRepeatedly(Return(0));
            EXPECT_CALL(*mock_result, UploadChunk(_))
                .WillRepeatedly(Return(google::cloud::Status(
                    google::cloud::StatusCode::kInvalidArgument,
                    "Invalid Argument")));
            EXPECT_CALL(*mock_result, UploadFinalChunk(_, _))
                .WillRepeatedly(Return(google::cloud::Status(
                    google::cloud::StatusCode::kInvalidArgument,
                    "Invalid Argument")));

            std::unique_ptr<gcs::internal::ResumableUploadSession> result(
                mock_result);
            return google::cloud::make_status_or(std::move(result));
          }));

  auto stream = client.WriteObject(bucket_name, object_name);
  stream << "Hello World!";
  stream.Close();

  if (stream.bad()) {
    std::cerr << "Error writing to stream... status="
              << stream.metadata().status() << std::endl;
  }

  //! [mock failed writeobject]
}

void RunAll(std::vector<std::string> const& argv) {
  if (!argv.empty()) throw Usage{"auto"};

  std::cout << "\nRunning the MockReadObject() example" << std::endl;
  MockReadObject({"test-bucket-name", "test-object-name"});

  std::cout << "\nRunning the MockWriteObject() example" << std::endl;
  MockWriteObject({"test-bucket-name", "test-object-name"});

  std::cout << "\nRunning the MockReadObjectFailure() example" << std::endl;
  MockReadObjectFailure({"test-bucket-name", "test-object-name"});

  std::cout << "\nRunning the MockWriteObjectFailure() example" << std::endl;
  MockWriteObjectFailure({"test-bucket-name", "test-object-name"});
}

}  // anonymous namespace

int main(int argc, char* argv[]) {
  namespace examples = ::google::cloud::storage::examples;
  google::cloud::storage::examples::Example example({
      {"mock-read-object", MockReadObject},
      {"mock-write-object", MockWriteObject},
      {"mock-read-object-failure", MockReadObjectFailure},
      {"mock-write-object-failure", MockWriteObjectFailure},
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
