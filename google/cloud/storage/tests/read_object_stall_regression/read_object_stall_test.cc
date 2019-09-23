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

#include "google/cloud/internal/big_endian.h"
#include "google/cloud/log.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <crc32c/crc32c.h>
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
double flag_average_delay = 1.0;
int flag_object_count = 32;
double flag_long_delay_rate = 0.10;
int flag_long_delay_seconds = 360;

struct ReadSummary {
  std::string bucket_name;
  std::string object_name;
  std::string received_hashes;
  std::string computed_hashes;
  std::size_t size = 0;
  std::uint32_t crc32c = 0;
  bool logged = false;
};

std::ostream& operator<<(std::ostream& os, ReadSummary const& x) {
  return os << "ReadSummary={bucket_name=" << x.bucket_name
            << ", object_name=" << x.object_name
            << ", received_hashes=" << x.received_hashes
            << ", computed_hashes=" << x.computed_hashes << ", size=" << x.size
            << ", crc32c=" << x.crc32c << ", logged=" << x.logged << "}";
}

void WriteSummary(Client client, std::string const& summary_name,
                  std::vector<ReadSummary> const& object_summaries) {
  std::ostringstream summary;
  for (auto const& s : object_summaries) {
    summary << s << "\n";
  }
  auto metadata =
      client.InsertObject(flag_dst_bucket_name, summary_name, summary.str());
  EXPECT_STATUS_OK(metadata);
}

void VerifySummary(Client client,
                   std::vector<ReadSummary> const& object_summaries) {
  int i = 0;
  for (auto const& summary : object_summaries) {
    SCOPED_TRACE("Checking headers and metadata for file [" +
                 std::to_string(i) + "]=" + summary.object_name);

    auto actual_crc32c = internal::Base64Encode(
        google::cloud::internal::EncodeBigEndian(summary.crc32c));

    auto metadata =
        client.GetObjectMetadata(summary.bucket_name, summary.object_name);
    ASSERT_STATUS_OK(metadata) << "ERROR: cannot read metadata for object[" << i
                               << "]=" << summary.object_name;

    EXPECT_EQ(metadata->size(), summary.size)
        << "ERROR: mismatched object size " << *metadata;
    EXPECT_EQ(metadata->crc32c(), actual_crc32c)
        << "ERROR: mismatched crc32c checksum " << *metadata;
    ++i;
  }
}

void UpdateFromReader(ReadSummary& read_summary, ObjectReadStream& r,
                      std::vector<char> const& buffer) {
  read_summary.size += r.gcount();
  read_summary.crc32c = crc32c::Extend(
      read_summary.crc32c, reinterpret_cast<std::uint8_t const*>(buffer.data()),
      r.gcount());

  EXPECT_FALSE(r.bad()) << "ERROR: bad bit detected: " << r.status();
  EXPECT_EQ(r.status().ok(), !r.bad())
      << "ERROR: mismatched status vs. bad: " << r.status();
  bool short_read = (static_cast<std::size_t>(r.gcount()) != buffer.size());
  EXPECT_EQ(r.fail(), short_read)
      << "mismatched fail and short read: " << r.status();
  EXPECT_EQ(r.eof(), short_read)
      << "mismatched eof and short read: " << r.status();

  if (!r.eof()) {
    return;
  }

  read_summary.received_hashes = r.received_hash();
  read_summary.computed_hashes = r.computed_hash();
  read_summary.logged = true;

  auto actual_crc32c = internal::Base64Encode(
      google::cloud::internal::EncodeBigEndian(read_summary.crc32c));

  auto const& headers = r.headers();
  auto object_size_begin = headers.lower_bound("x-goog-stored-content-length");
  EXPECT_TRUE(object_size_begin != headers.end())
      << "ERROR: cloud not x-goog-stored-content-length header";
  if (object_size_begin != headers.end()) {
    auto expected_object_size = std::stoll(object_size_begin->second);
    EXPECT_EQ(expected_object_size, read_summary.size)
        << "ERROR: mismatched x-goog-stored-content-length header"
        << " vs. received size " << r.received_hash()
        << ", computed=" << r.computed_hash();
  }
  auto x_goog_hash_begin = headers.lower_bound("x-goog-hash");
  auto x_goog_hash_end = headers.upper_bound("x-goog-hash");
  bool found = false;
  for (auto h = x_goog_hash_begin; h != x_goog_hash_end; ++h) {
    if (h->second.rfind("crc32c=", 0) == 0) {
      EXPECT_THAT(h->second, HasSubstr(actual_crc32c))
          << "ERROR: mismatched x-goog-hash header for crc32c and "
             "computed "
             "crc32c checksum "
          << r.received_hash() << ", computed=" << r.computed_hash();
      found = true;
    }
  }
  EXPECT_TRUE(found) << "ERROR: could not find x-goog-hash header with "
                        "crc32c checksum";
}

}  // namespace

class ReadObjectStallTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  ReadObjectStallTest()
      : sleep_period_(1.0 / flag_average_delay), use_long_delay_(0, 1) {}

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

  std::chrono::seconds Delay() {
    if (use_long_delay_(generator_) <= flag_long_delay_rate) {
      std::cout << '+' << std::flush;
      return std::chrono::seconds(flag_long_delay_seconds);
    }
    return std::chrono::seconds(sleep_period_(generator_));
  }

  std::vector<ReadSummary> ReadStreaming(
      Client client, std::vector<std::string> const& object_names) {
    std::vector<ReadSummary> read_summaries(object_names.size());
    auto summary_name = MakeRandomObjectName();

    std::vector<ObjectReadStream> readers;
    readers.reserve(object_names.size());
    std::size_t index = 0;
    for (auto const& name : object_names) {
      read_summaries[index].bucket_name = flag_src_bucket_name;
      read_summaries[index].object_name = name;
      ++index;
      readers.emplace_back(client.ReadObject(flag_src_bucket_name, name));
    }

    std::size_t offset = 0;
    int open_count;
    do {
      open_count = 0;
      std::size_t const chunk_size = 1 * 1024 * 1024L;
      std::vector<char> buffer(chunk_size);
      index = 0;
      for (auto& r : readers) {
        auto& read_summary = read_summaries[index++];
        if (!r.IsOpen()) {
          continue;
        }
        r.read(buffer.data(), buffer.size());
        UpdateFromReader(read_summary, r, buffer);
        ++open_count;
      }
      offset += chunk_size;
      // Flush because we want to see the output in the GKE logs.
      std::cout << '.' << std::flush;
      std::this_thread::sleep_for(Delay());
      WriteSummary(client, summary_name, read_summaries);
    } while (open_count != 0);
    std::cout << "DONE: All files closed\n";
    return read_summaries;
  }

  std::vector<ReadSummary> ReadByRange(
      Client client, std::vector<std::string> const& object_names) {
    auto summary_name = MakeRandomObjectName();
    struct Reader {
      std::size_t offset = 0;
      bool closed = false;
    };
    std::vector<ReadSummary> read_summaries(object_names.size());
    std::vector<Reader> readers(object_names.size());

    std::size_t index = 0;
    for (auto const& name : object_names) {
      read_summaries[index].bucket_name = flag_src_bucket_name;
      read_summaries[index].object_name = name;
      ++index;
    }

    std::geometric_distribution<int> sleep_period(1.0 / flag_average_delay);

    std::size_t offset = 0;
    int open_count;
    do {
      open_count = 0;
      std::size_t const chunk_size = 1 * 1024 * 1024L;
      std::vector<char> buffer(chunk_size);
      index = 0;
      for (auto& reader : readers) {
        auto& read_summary = read_summaries[index++];
        if (reader.closed) {
          continue;
        }
        auto r = client.ReadObject(
            read_summary.bucket_name, read_summary.object_name,
            ReadRange(reader.offset, reader.offset + chunk_size));
        r.read(buffer.data(), buffer.size());
        UpdateFromReader(read_summary, r, buffer);
        ++open_count;
        reader.offset += r.gcount();
        reader.closed = r.eof();
      }
      offset += chunk_size;
      // Flush because we want to see the output in the GKE logs.
      std::cout << '.' << std::flush;
      std::this_thread::sleep_for(Delay());
      WriteSummary(client, summary_name, read_summaries);
    } while (open_count != 0);
    std::cout << "DONE: All files closed\n";
    return read_summaries;
  }

  std::vector<ReadSummary> ReadByFile(
      Client client, std::vector<std::string> const& object_names) {
    auto summary_name = MakeRandomObjectName();

    std::vector<ReadSummary> read_summaries(object_names.size());

    std::size_t index = 0;
    for (auto const& name : object_names) {
      read_summaries[index].bucket_name = flag_src_bucket_name;
      read_summaries[index].object_name = name;
      ++index;
    }

    int max_chunks = 0;
    for (auto& read_summary : read_summaries) {
      std::size_t const chunk_size = 1 * 1024 * 1024L;
      std::vector<char> buffer(chunk_size);
      auto r =
          client.ReadObject(read_summary.bucket_name, read_summary.object_name);
      int chunks = 0;
      while (!r.eof() && !r.bad()) {
        r.read(buffer.data(), buffer.size());
        UpdateFromReader(read_summary, r, buffer);
        ++chunks;
      }
      max_chunks = (std::max)(chunks, max_chunks);
      std::cout << "Reading done for " << read_summary.object_name
                << ", max_chunks=" << max_chunks << "\n"
                << std::flush;
    }
    std::geometric_distribution<int> sleep_period(1.0 / flag_average_delay);
    std::uniform_real_distribution<double> use_long_delay(0.0, 1.0);
    for (int i = 0; i != max_chunks; ++i) {
      auto sleep_seconds = Delay();
      std::cout << "Sleeping for " << sleep_seconds.count() << "s\n";
      std::this_thread::sleep_for(sleep_seconds);
      WriteSummary(client, summary_name, read_summaries);
    }

    std::cout << "DONE\n";
    return read_summaries;
  }

 private:
  std::geometric_distribution<int> sleep_period_;
  std::uniform_real_distribution<double> use_long_delay_;
};

TEST_F(ReadObjectStallTest, Streaming) {
  auto options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(options)
      << "ERROR: Aborting test, cannot create client options";

  Client client(options->set_maximum_socket_recv_size(128 * 1024)
                    .set_maximum_socket_send_size(128 * 1024)
                    .set_download_stall_timeout(std::chrono::seconds(30)));

  PreparePhase(client, 1000);

  auto object_names = PickObjects(client, flag_object_count);
  auto download_summaries = ReadStreaming(client, object_names);
  EXPECT_NO_FATAL_FAILURE(VerifySummary(client, download_summaries));
}

TEST_F(ReadObjectStallTest, ByRange) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client) << "ERROR: Aborting test, cannot create client";

  PreparePhase(*client, 1000);
  auto object_names = PickObjects(*client, flag_object_count);
  auto download_summaries = ReadByRange(*client, object_names);
  EXPECT_NO_FATAL_FAILURE(VerifySummary(*client, download_summaries));
}

TEST_F(ReadObjectStallTest, ByFile) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client) << "ERROR: Aborting test, cannot create client";

  PreparePhase(*client, 1000);
  auto object_names = PickObjects(*client, flag_object_count);
  auto download_summaries = ReadByFile(*client, object_names);
  EXPECT_NO_FATAL_FAILURE(VerifySummary(*client, download_summaries));
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InitGoogleMock(argc, argv);

  // Make sure the arguments are valid.
  if (argc != 4 && argc != 5) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <source-bucket-name> <destination-bucket-name>"
                 " <average-delay> [object-count]\n";
    return 1;
  }

  google::cloud::storage::flag_src_bucket_name = argv[1];
  google::cloud::storage::flag_dst_bucket_name = argv[2];
  google::cloud::storage::flag_average_delay = std::stod(argv[3]);
  if (argc > 4) {
    google::cloud::storage::flag_object_count = std::stol(argv[4]);
  }

  return RUN_ALL_TESTS();
}
