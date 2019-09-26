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
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/capture_log_lines_backend.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>
#include <cstdio>
#include <fstream>
#include <future>
#include <regex>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::testing::HasSubstr;

// Initialized in main() below.
char const* flag_bucket_name;
int flag_object_count;

auto const kObjectSize = 16 * 1024;

class ObjectFileMultiThreadedTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  unsigned ThreadCount() {
    static unsigned const count = [] {
      auto c = std::thread::hardware_concurrency();
      return c == 0 ? 4u : 4 * c;
    }();
    return count;
  }

  std::vector<std::string> CreateObjectNames() {
    std::vector<std::string> object_names(flag_object_count);
    std::generate_n(object_names.begin(), object_names.size(),
                    [this] { return MakeRandomObjectName(); });
    return object_names;
  }

  Status CreateSomeObjects(Client client,
                           std::vector<std::string> const& object_names,
                           int thread_count, int modulo) {
    auto contents = [this] {
      std::unique_lock<std::mutex> lk(mu_);
      return MakeRandomData(kObjectSize);
    }();
    int index = 0;
    for (auto& n : object_names) {
      if (index++ % thread_count != modulo) continue;
      if (modulo == 0) {
        std::unique_lock<std::mutex> lk(mu_);
        std::cout << '.' << std::flush;
      }
      auto metadata = client.InsertObject(flag_bucket_name, n, contents,
                                          IfGenerationMatch(0));
      if (!metadata) return metadata.status();
    }
    return Status();
  }

  void CreateObjects(Client const& client,
                     std::vector<std::string> const& object_names) {
    // Parallelize the object creation too because it can be slow.
    int const thread_count = ThreadCount();
    auto create_some_objects = [this, &client, &object_names,
                                thread_count](int modulo) {
      return CreateSomeObjects(client, object_names, thread_count, modulo);
    };
    std::vector<std::future<Status>> tasks(thread_count);
    int modulo = 0;
    for (auto& t : tasks) {
      t = std::async(std::launch::async, create_some_objects, modulo++);
    }
    for (auto& t : tasks) {
      auto const status = t.get();
      EXPECT_STATUS_OK(status);
    }
  }

  Status DeleteSomeObjects(Client client,
                           std::vector<std::string> const& object_names,
                           int thread_count, int modulo) {
    auto contents = MakeRandomData(kObjectSize);
    int index = 0;
    Status status;
    for (auto& name : object_names) {
      if (index++ % thread_count != modulo) continue;
      if (modulo == 0) {
        std::unique_lock<std::mutex> lk(mu_);
        std::cout << '.' << std::flush;
      }
      auto result = client.DeleteObject(flag_bucket_name, name);
      if (!result.ok()) status = result;
    }
    return status;
  }

  void DeleteObjects(Client const& client,
                     std::vector<std::string> const& object_names) {
    // Parallelize the object deletion too because it can be slow.
    int const thread_count = ThreadCount();
    auto create_some_objects = [this, &client, &object_names,
                                thread_count](int modulo) {
      return DeleteSomeObjects(client, object_names, thread_count, modulo);
    };
    std::vector<std::future<Status>> tasks(thread_count);
    int modulo = 0;
    for (auto& t : tasks) {
      t = std::async(std::launch::async, create_some_objects, modulo++);
    }
    for (auto& t : tasks) {
      auto const status = t.get();
      EXPECT_STATUS_OK(status);
    }
  }

 protected:
  std::mutex mu_;
};

TEST_F(ObjectFileMultiThreadedTest, Download) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto const object_names = CreateObjectNames();
  std::cout << "Create test objects " << std::flush;
  ASSERT_NO_FATAL_FAILURE(CreateObjects(*client, object_names));
  std::cout << " DONE\n";

  // Create multiple threads, each downloading a portion of the objects.
  int const thread_count = ThreadCount();
  auto download_some_objects = [this, thread_count, &client,
                                &object_names](int modulo) {
    std::cout << '+' << std::flush;
    auto contents = MakeRandomData(kObjectSize);
    int index = 0;
    for (auto& name : object_names) {
      if (index++ % thread_count != modulo) continue;
      if (modulo == 0) {
        std::unique_lock<std::mutex> lk(mu_);
        std::cout << '.' << std::flush;
      }
      auto status = client->DownloadToFile(flag_bucket_name, name, name);
      if (!status.ok()) return status;  // stop on the first error
    }
    return Status();
  };
  std::cout << "Performing downloads " << std::flush;
  std::vector<std::future<Status>> tasks(thread_count);
  int modulo = 0;
  for (auto& t : tasks) {
    t = std::async(std::launch::async, download_some_objects, modulo++);
  }
  for (auto& t : tasks) {
    auto const status = t.get();
    EXPECT_STATUS_OK(status);
  }
  std::cout << " DONE\n";

  for (auto& name : object_names) {
    EXPECT_EQ(0, std::remove(name.c_str()));
  }

  std::cout << "Delete test objects " << std::flush;
  ASSERT_NO_FATAL_FAILURE(DeleteObjects(*client, object_names));
  std::cout << " DONE\n";
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
              << " <bucket-name> <object-count>\n";
    return 1;
  }

  google::cloud::storage::flag_bucket_name = argv[1];
  google::cloud::storage::flag_object_count = std::stoi(argv[2]);

  return RUN_ALL_TESTS();
}
