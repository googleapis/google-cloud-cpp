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
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::testing::HasSubstr;

// Initialized in main() below.
char const* flag_src_bucket_name;
char const* flag_dst_bucket_name;
double flag_average_delay;

class ReadObjectStallTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  std::vector<std::string> GetObjectNames(Client client) {
    std::vector<std::string> object_names;
    for (auto const& object : client.ListObjects(flag_src_bucket_name)) {
      if (!object) {
        return object_names;
      }
      object_names.push_back(object->name());
    }
    return object_names;
  }

  std::size_t CurrentObjectCount(Client const& client) {
    return GetObjectNames(client).size();
  }

  void AddSomeObjects(Client client) {
    auto const min_object_size = 32 * 1024 * 1024L;
    auto const max_object_size = 64 * 1024 * 1024L;
    std::string contents = MakeRandomData(max_object_size);
    std::uniform_int_distribution<long> object_size(min_object_size,
                                                    max_object_size);

    int const object_count = 16;
    std::vector<std::string> object_names(object_count);
    std::generate_n(object_names.begin(), object_count,
                    [this] { return MakeRandomObjectName(); });
    std::cout << "Creating objects to read " << std::flush;
    for (auto const& name : object_names) {
      auto size = object_size(generator_);
      auto object_metadata = client.InsertObject(flag_src_bucket_name, name,
                                                 contents.substr(0, size));
      if (!object_metadata) {
        continue;
      }
      std::cout << '.' << std::flush;
    }
    std::cout << " DONE\n" << std::flush;
  }

  void PreparePhase(Client const& client, std::size_t desired_object_count) {
    while (CurrentObjectCount(client) < desired_object_count) {
      AddSomeObjects(client);
    }
  }

  std::vector<std::string> PickObjects(Client client, int object_count) {
    auto all_object_names = GetObjectNames(client);
    std::uniform_int_distribution<std::size_t> pick(
        0, all_object_names.size() - 1);

    std::vector<std::string> object_names(object_count);
    std::generate_n(object_names.begin(), object_count,
                    [&] { return all_object_names.at(pick(generator_)); });
    return object_names;
  }
};

TEST_F(ReadObjectStallTest, Streaming) {
  // Give the other programs in the test (such as tcpdump) time to start up.
  std::this_thread::sleep_for(std::chrono::seconds(30));

  auto options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(options)
      << "ERROR: Aborting test, cannot create client options";

  Client client(options->set_maximum_socket_recv_size(128 * 1024)
                    .set_maximum_socket_send_size(128 * 1024)
                    .set_download_stall_timeout(std::chrono::seconds(30)));

  PreparePhase(client, 1000);

  auto summary_name = MakeRandomObjectName();

  auto object_names = PickObjects(client, 32);
  std::vector<ObjectReadStream> readers;
  readers.reserve(object_names.size());
  for (auto const& name : object_names) {
    readers.emplace_back(client.ReadObject(flag_src_bucket_name, name));
  }

  std::geometric_distribution<int> sleep_period(1.0 / flag_average_delay);

  bool has_open;
  std::size_t offset = 0;
  do {
    has_open = false;
    std::size_t const chunk_size = 1 * 1024 * 1024L;
    std::vector<char> buffer(chunk_size);
    int open_count = 0;
    for (auto& r : readers) {
      if (!r.IsOpen()) {
        continue;
      }
      has_open = true;
      r.read(buffer.data(), buffer.size());
      if (r.bad()) {
        std::cerr << "Bad bit detected: " << r.status() << "\n";
      }
      ++open_count;
    }
    offset += chunk_size;
    // Flush because we want to see the output in the GKE logs.
    int sleep_seconds = sleep_period(generator_);
    std::cout << "Reading, still has " << open_count
              << " open files at offset=" << offset << "\n"
              << "Sleeping for " << sleep_seconds << "s\n"
              << std::flush;
    std::this_thread::sleep_for(std::chrono::seconds(sleep_seconds));

    std::ostringstream summary;
    std::size_t index = 0;
    for (auto const& o : object_names) {
      summary << o << " " << index;
      if (index < readers.size()) {
        auto const& r = readers[index];
        summary << " " << r.IsOpen() << " " << r.computed_hash() << " "
                << r.headers().size();
      }
      summary << "\n";
      ++index;
    }
    auto metadata =
        client.InsertObject(flag_dst_bucket_name, summary_name, summary.str());
    EXPECT_STATUS_OK(metadata);
  } while (has_open);
  std::cout << "DONE: All files closed\n";
}

TEST_F(ReadObjectStallTest, ByRange) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client) << "ERROR: Aborting test, cannot create client";

  PreparePhase(*client, 1000);
  auto summary_name = MakeRandomObjectName();

  struct State {
    bool closed;
    std::size_t offset;
    std::string name;
  };
  std::vector<State> readers;
  readers.reserve(32);
  for (auto const& name : PickObjects(*client, 32)) {
    readers.emplace_back(State{false, 0, name});
  }

  std::geometric_distribution<int> sleep_period(1.0 / flag_average_delay);

  bool has_open;
  std::size_t offset = 0;
  do {
    has_open = false;
    std::size_t const chunk_size = 1 * 1024 * 1024L;
    std::vector<char> buffer(chunk_size);
    int open_count = 0;
    for (auto& state : readers) {
      if (state.closed) {
        continue;
      }
      auto r = client->ReadObject(
          flag_src_bucket_name, state.name,
          ReadRange(state.offset, state.offset + chunk_size));
      EXPECT_FALSE(r.eof());
      EXPECT_FALSE(r.bad()) << "status=" << r.status();
      if (r.bad() || r.eof()) {
        state.closed = true;
        continue;
      }
      has_open = true;
      r.read(buffer.data(), buffer.size());
      state.offset += r.gcount();
      if (r.eof()) {
        state.closed = true;
      }
      EXPECT_FALSE(r.bad()) << "status=" << r.status() << "\n";
      ++open_count;
    }
    offset += chunk_size;
    // Flush because we want to see the output in the GKE logs.
    int sleep_seconds = sleep_period(generator_);
    std::cout << "Reading, still has " << open_count
              << " open files at offset=" << offset << "\n"
              << "Sleeping for " << sleep_seconds << "s\n"
              << std::flush;
    std::this_thread::sleep_for(std::chrono::seconds(sleep_seconds));

    std::ostringstream summary;
    std::size_t index = 0;
    for (auto const& s : readers) {
      summary << s.name << " " << index << " " << s.closed << " " << s.offset
              << "\n";
      ++index;
    }
    auto metadata =
        client->InsertObject(flag_dst_bucket_name, summary_name, summary.str());
    EXPECT_STATUS_OK(metadata);
  } while (has_open);
  std::cout << "DONE: All files closed\n";
}

TEST_F(ReadObjectStallTest, ByFile) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client) << "ERROR: Aborting test, cannot create client";

  PreparePhase(*client, 1000);

  auto summary_name = MakeRandomObjectName();

  auto object_names = PickObjects(*client, 32);
  int max_chunks = 0;
  for (auto const& name : object_names) {
    auto reader = client->ReadObject(flag_src_bucket_name, name);
    std::size_t const chunk_size = 1 * 1024 * 1024L;
    std::vector<char> buffer(chunk_size);
    int chunks = 0;
    while (!reader.eof() && !reader.bad()) {
      reader.read(buffer.data(), buffer.size());
      EXPECT_FALSE(reader.bad());
      EXPECT_TRUE(buffer.size() == static_cast<std::size_t>(reader.gcount()) ||
                  reader.eof());
      ++chunks;
    }
    EXPECT_FALSE(reader.bad());
    EXPECT_STATUS_OK(reader.status());
    max_chunks = (std::max)(chunks, max_chunks);
    std::cout << "Reading done for " << name << ", max_chunks=" << max_chunks
              << "\n"
              << std::flush;
  }

  std::geometric_distribution<int> sleep_period(1.0 / flag_average_delay);
  for (int i = 0; i != max_chunks; ++i) {
    int sleep_seconds = sleep_period(generator_);
    std::cout << "Sleeping for " << sleep_seconds << "s\n" << std::flush;
    std::this_thread::sleep_for(std::chrono::seconds(sleep_seconds));

    auto meta =
        client->InsertObject(flag_dst_bucket_name, summary_name, "test only");
    EXPECT_STATUS_OK(meta);
  }

  std::cout << "DONE\n";
}

}  // anonymous namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InitGoogleMock(argc, argv);

  // Make sure the arguments are valid.
  if (argc != 4) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <source-bucket-name> <destination-bucket-name>"
                 " <average-delay>\n";
    return 1;
  }

  google::cloud::storage::flag_src_bucket_name = argv[1];
  google::cloud::storage::flag_dst_bucket_name = argv[2];
  google::cloud::storage::flag_average_delay = std::stod(argv[3]);

  return RUN_ALL_TESTS();
}
