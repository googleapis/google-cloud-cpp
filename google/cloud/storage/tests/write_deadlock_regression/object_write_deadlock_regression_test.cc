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

#include "google/cloud/log.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/storage/tests/write_deadlock_regression/backtrace.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>
#include <thread>
#include <unistd.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::testing::HasSubstr;

// Initialized in main() below.
char const* flag_project_id;
char const* flag_bucket_name;

std::atomic_bool ignore_alarms(true);

extern "C" void alarm_handler(int) {
  if (ignore_alarms.load()) {
    return;
  }

  int const array_size = 32;
  void* array[array_size];

  // Get the address of the current stack.
  auto size = ::backtrace(array, array_size);

  std::fprintf(stderr, "ERROR: Request timed out, stack trace follows...\n");
  ::backtrace_symbols_fd(array, size, STDERR_FILENO);
  std::fprintf(stderr, "============\n");
  std::exit(1);
}

class ObjectWriteDeadlockRegressionTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  std::vector<std::string> PreparePhase(Client client, int source_count) {
    std::vector<std::string> object_names(source_count);
    std::generate_n(object_names.begin(), source_count,
                    [this] { return MakeRandomObjectName(); });
    std::string contents = MakeRandomData(16 * 1024 * 1024);
    std::cout << "Creating objects to read " << std::flush;
    for (auto const& name : object_names) {
      auto object_metadata = client.InsertObject(
          flag_bucket_name, name, contents, IfGenerationMatch(0));
      if (!object_metadata) {
        EXPECT_STATUS_OK(object_metadata) << "ERROR: Cannot create object"
                                          << ", name=" << name;
        std::exit(1);
      }
      std::cout << '.' << std::flush;
    }
    std::cout << " DONE\n" << std::flush;
    return object_names;
  }

  void ReadPhase(Client client, std::vector<std::string> const& object_names) {
    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(30);

    std::cout << "\nReading from all objects " << std::flush;
    while (std::chrono::system_clock::now() < deadline) {
      std::vector<ObjectReadStream> readers;
      for (auto const& name : object_names) {
        readers.emplace_back(client.ReadObject(flag_bucket_name, name));
      }

      bool has_open;
      do {
        has_open = false;
        std::vector<char> buffer(1 * 1024 * 1024);
        for (auto& r : readers) {
          if (!r.IsOpen()) {
            continue;
          }
          has_open = true;
          r.read(buffer.data(), buffer.size());
        }
        std::cout << '.' << std::flush;
      } while (has_open);
      std::cout << '+' << std::flush;
    }
    std::cout << " DONE\n" << std::flush;
  }

  std::string WritePhase() {
    StatusOr<Client> client = Client::CreateDefaultClient();
    EXPECT_STATUS_OK(client) << "ERROR: Aborting test, cannot create client";
    if (!client) {
      std::exit(1);
    }

    auto object_name = MakeRandomObjectName();

    std::string expected = MakeRandomData(256 * 1024);

    ignore_alarms.store(false);
    ::signal(SIGALRM, &alarm_handler);
    ::alarm(120);  // send an alarm signal after 120 seconds.

    // Create the object, but only if it does not exist already.
    auto writer = client->WriteObject(flag_bucket_name, object_name,
                                      IfGenerationMatch(0));
    writer.write(expected.data(), expected.size());
    writer.Close();
    EXPECT_STATUS_OK(writer.metadata());
    ignore_alarms.store(true);
    ::signal(SIGALRM, SIG_IGN);

    return object_name;
  }
};

TEST_F(ObjectWriteDeadlockRegressionTest, StreamingWrite) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client) << "ERROR: Aborting test, cannot create client";

  auto object_names = PreparePhase(*client, 32);
  ReadPhase(*client, object_names);
  // Remove before running the write phase.
  for (auto const& name : object_names) {
    auto status = client->DeleteObject(flag_bucket_name, name);
    EXPECT_STATUS_OK(status);
  }

  std::string object_name = WritePhase();
  // Cleanup after running the test.
  auto status = client->DeleteObject(flag_bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

}  // anonymous namespace
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
              << " <project-id> <bucket-name>\n";
    return 1;
  }

  google::cloud::storage::flag_project_id = argv[1];
  google::cloud::storage::flag_bucket_name = argv[2];

  return RUN_ALL_TESTS();
}
