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
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/storage/well_known_parameters.h"
#include "google/cloud/internal/make_unique.h"
#include <gmock/gmock.h>
#include <functional>
#include <iostream>
#include <map>
#include <string>

namespace {
struct Usage {
  std::string msg;
};

namespace gcs = google::cloud::storage;
using ::google::cloud::StatusOr;

char const* ConsumeArg(int& argc, char* argv[]) {
  if (argc < 2) {
    return nullptr;
  }
  char const* result = argv[1];
  std::copy(argv + 2, argv + argc, argv + 1);
  argc--;
  return result;
}

std::string command_usage;

void PrintUsage(int, char* argv[], std::string const& msg) {
  std::string const cmd = argv[0];
  auto last_slash = std::string(cmd).find_last_of('/');
  auto program = cmd.substr(last_slash + 1);
  std::cerr << msg << "\nUsage: " << program << " <command> [arguments]\n\n"
            << "Commands:\n"
            << command_usage << "\n";
}

void MockReadObject(int& argc, char* argv[]) {
  if (argc != 3) {
    throw Usage{"mock-read-object <bucket-name> <object-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);

  //! [mock successful readobject]
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

void MockWriteObject(int& argc, char* argv[]) {
  if (argc != 3) {
    throw Usage{"mock-write-object <bucket-name> <object-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);

  //! [mock successful writeobject]
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

void MockReadObjectFailure(int& argc, char* argv[]) {
  if (argc != 3) {
    throw Usage{"mock-read-object-failure <bucket-name> <object-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);

  //! [mock failed readobject]
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

void MockWriteObjectFailure(int& argc, char* argv[]) {
  if (argc != 3) {
    throw Usage{"mock-write-object-failure <bucket-name> <object-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);

  //! [mock failed writeobject]
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

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  using CommandType = std::function<void(int&, char*[])>;
  std::map<std::string, CommandType> commands = {
      {"mock-read-object", MockReadObject},
      {"mock-write-object", MockWriteObject},
      {"mock-read-object-failure", MockReadObjectFailure},
      {"mock-write-object-failure", MockWriteObjectFailure},
  };
  for (auto&& kv : commands) {
    try {
      int fake_argc = 0;
      kv.second(fake_argc, argv);
    } catch (Usage const& u) {
      command_usage += "    ";
      command_usage += u.msg;
      command_usage += "\n";
    }
  }

  if (argc < 2) {
    PrintUsage(argc, argv, "Missing command");
    return 1;
  }

  std::string const command = ConsumeArg(argc, argv);
  auto it = commands.find(command);
  if (commands.end() == it) {
    PrintUsage(argc, argv, "Unknown command: " + command);
    return 1;
  }

  it->second(argc, argv);

  return 0;
} catch (Usage const& ex) {
  PrintUsage(argc, argv, ex.msg);
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << "\n";
  return 1;
}
