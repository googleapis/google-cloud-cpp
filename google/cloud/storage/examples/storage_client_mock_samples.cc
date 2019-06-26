// Copyright 2018 Google LLC
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

#include "google/cloud/internal/make_unique.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/storage/well_known_parameters.h"
#include <gmock/gmock.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
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

  std::shared_ptr<gcs::testing::MockClient> mock;
  std::unique_ptr<gcs::Client> client;

  gcs::ClientOptions client_options =
      gcs::ClientOptions(gcs::oauth2::CreateAnonymousCredentials());

  mock = std::make_shared<gcs::testing::MockClient>();
  EXPECT_CALL(*mock, client_options())
      .WillRepeatedly(testing::ReturnRef(client_options));
  client.reset(
      new gcs::Client{std::shared_ptr<gcs::internal::RawClient>(mock)});

  std::string text = "this is a mock http response";

  EXPECT_CALL(*mock, ReadObject(testing::_))
      .WillOnce(testing::Invoke(
          [&text](gcs::internal::ReadObjectRangeRequest const& request) {
            std::cout << "\nReading from bucket : " << request.bucket_name()
                      << std::endl;
            auto* mock_source = new gcs::testing::MockObjectReadSource;
            EXPECT_CALL(*mock_source, IsOpen())
                .WillOnce(testing::Return(true))
                .WillRepeatedly(testing::Return(false));
            EXPECT_CALL(*mock_source, Read(testing::_, testing::_))
                .WillOnce(testing::Return(gcs::internal::ReadSourceResult{
                    text.length(),
                    gcs::internal::HttpResponse{200, text, {}}}));

            std::unique_ptr<gcs::internal::ObjectReadSource> result(
                mock_source);

            return google::cloud::make_status_or(std::move(result));
          }));

  [](gcs::Client client, std::string bucket_name, std::string object_name) {
    gcs::ObjectReadStream stream = client.ReadObject(bucket_name, object_name);

    // Reading the payload of http response stored in stream
    int count = 0;
    std::string line;
    while (std::getline(stream, line, '\n')) {
      std::cout << "Reading line : " << line << std::endl;
      ++count;
    }

    std::cout << "The object has " << count << " line(s)\n";
  }(*client, bucket_name, object_name);
}

void MockWriteObject(int& argc, char* argv[]) {
  if (argc != 3) {
    throw Usage{"mock-write-object <bucket-name> <object-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);

  std::shared_ptr<gcs::testing::MockClient> mock;
  std::unique_ptr<gcs::Client> client;
  gcs::ClientOptions client_options =
      gcs::ClientOptions(gcs::oauth2::CreateAnonymousCredentials());

  mock = std::make_shared<gcs::testing::MockClient>();
  EXPECT_CALL(*mock, client_options())
      .WillRepeatedly(testing::ReturnRef(client_options));
  client.reset(
      new gcs::Client{std::shared_ptr<gcs::internal::RawClient>(mock)});

  std::string text = R"""({
        "name": "test-bucket-name/test-object-name/1"
  })""";
  auto expected = gcs::internal::ObjectMetadataParser::FromString(text).value();

  EXPECT_CALL(*mock, WriteObject(testing::_))
      .WillOnce(testing::Invoke(
          [&text](gcs::internal::InsertObjectStreamingRequest const& request) {
            std::cout << "Writing to bucket : " << request.bucket_name()
                      << std::endl;
            auto* mock_result = new gcs::testing::MockStreambuf;
            EXPECT_CALL(*mock_result, DoClose())
                .WillRepeatedly(testing::Return(
                    gcs::internal::HttpResponse{200, text, {}}));
            EXPECT_CALL(*mock_result, IsOpen())
                .WillRepeatedly(testing::Return(true));
            EXPECT_CALL(*mock_result, ValidateHash(testing::_))
                .WillRepeatedly(testing::Return(true));
            std::unique_ptr<gcs::internal::ObjectWriteStreambuf> result(
                mock_result);
            return google::cloud::make_status_or(std::move(result));
          }));

  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string object_name) {
    auto stream = client.WriteObject(bucket_name, object_name);
    stream << "Hello World!";
    stream.Close();
  }(*client, bucket_name, object_name);
}

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  using CommandType = std::function<void(int&, char*[])>;
  std::map<std::string, CommandType> commands = {
      {"mock-read-object", &MockReadObject},
      {"mock-write-object", &MockWriteObject},
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
