// Copyright 2020 Google LLC
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

#include "google/cloud/internal/build_info.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/storage/benchmarks/benchmark_utils.h"
#include "google/cloud/storage/benchmarks/bounded_queue.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/curl_wrappers.h"
#include <curl/multi.h>
#include <future>
#include <iomanip>
#include <sstream>

namespace {
namespace gcs = google::cloud::storage;
namespace gcs_bm = google::cloud::storage_benchmarks;

char const kDescription[] = R"""(
Prototype asynchronous reads for the Google Cloud Storage C++ client library.

Many applications seem to read many small ranges from several objects more or
less simultaneously. We think that using many curl_easy_perform() requests
over the same CURLM* interface would have some benefits over multiple threads
using storage::Client::ReadStream().  If this is true, then we should consider
exposing an API in the library to perform such reads.
)""";

struct Options {
  std::string project_id;
  std::string bucket_prefix = "cloud-cpp-testing-";
  std::string region;
  int object_count = 100;
  int thread_count = 1;
  int iteration_size = 100;
  int iteration_count = 100;
  std::int64_t chunk_size = 12 * gcs_bm::kMiB;
  int chunk_count = 20;
};

struct WorkItem {
  std::string bucket;
  std::string object;
  std::int64_t begin;
  std::int64_t end;
};

using WorkItemQueue = gcs_bm::BoundedQueue<std::vector<WorkItem>>;

struct IterationResult {
  std::int64_t bytes_requested;
  std::int64_t bytes_received;
  std::chrono::microseconds elapsed;
};

std::vector<std::string> CreateAllObjects(
    gcs::Client client, google::cloud::internal::DefaultPRNG& gen,
    std::string const& bucket_name, Options const& options);

IterationResult RunOneIteration(google::cloud::internal::DefaultPRNG& generator,
                                Options const& options,
                                std::string const& bucket_name,
                                std::vector<std::string> const& object_names);

void DeleteAllObjects(gcs::Client client, std::string const& bucket_name,
                      Options const& options,
                      std::vector<std::string> const& object_names);

google::cloud::StatusOr<Options> ParseArgs(int argc, char* argv[]);

}  // namespace

int main(int argc, char* argv[]) {
  google::cloud::StatusOr<Options> options = ParseArgs(argc, argv);
  if (!options) {
    std::cerr << options.status() << "\n";
    return 1;
  }

  google::cloud::StatusOr<gcs::ClientOptions> client_options =
      gcs::ClientOptions::CreateDefaultClientOptions();
  if (!client_options) {
    std::cerr << "Could not create ClientOptions, status="
              << client_options.status() << "\n";
    return 1;
  }
  if (!options->project_id.empty()) {
    client_options->set_project_id(options->project_id);
  }
  gcs::Client client(*std::move(client_options));

  google::cloud::internal::DefaultPRNG generator =
      google::cloud::internal::MakeDefaultPRNG();

  auto bucket_name =
      gcs_bm::MakeRandomBucketName(generator, options->bucket_prefix);
  std::cout << "# Creating bucket " << bucket_name << " in region "
            << options->region << "\n";
  auto meta =
      client
          .CreateBucket(bucket_name,
                        gcs::BucketMetadata()
                            .set_storage_class(gcs::storage_class::Standard())
                            .set_location(options->region),
                        gcs::PredefinedAcl("private"),
                        gcs::PredefinedDefaultObjectAcl("projectPrivate"),
                        gcs::Projection("full"))
          .value();
  std::cout << "# Running test on bucket: " << meta.name() << "\n";
  std::string notes = google::cloud::storage::version_string() + ";" +
                      google::cloud::internal::compiler() + ";" +
                      google::cloud::internal::compiler_flags();
  std::transform(notes.begin(), notes.end(), notes.begin(),
                 [](char c) { return c == '\n' ? ';' : c; });
  std::cout << "# Start time: "
            << google::cloud::internal::FormatRfc3339(
                   std::chrono::system_clock::now())
            << "\n# Region: " << options->region
            << "\n# Object Count: " << options->object_count
            << "\n# Thread Count: " << options->thread_count
            << "\n# Iteration Size: " << options->iteration_size
            << "\n# Iteration Count: " << options->iteration_count
            << "\n# Chunk Size: " << options->chunk_size
            << "\n# Chunk Size (MiB): " << options->chunk_size / gcs_bm::kMiB
            << "\n# Chunk Count: " << options->chunk_count
            << "\n# Build info: " << notes << std::endl;

  std::vector<std::string> const object_names =
      CreateAllObjects(client, generator, bucket_name, *options);

  double MiBs_sum = 0.0;
  double Gbps_sum = 0.0;
  for (long i = 0; i != options->iteration_count; ++i) {
    auto const r =
        RunOneIteration(generator, *options, bucket_name, object_names);
    std::cout << r.bytes_requested << ',' << r.bytes_received << ','
              << r.elapsed.count() << std::endl;
    auto const MiB = r.bytes_received / gcs_bm::kMiB;
    auto const MiBs =
        MiB * (1.0 * decltype(r.elapsed)::period::den) / r.elapsed.count();
    MiBs_sum += MiBs;
    auto const Gb = r.bytes_received * 8.0 / gcs_bm::kGB;
    auto const Gbps =
        Gb * (1.0 * decltype(r.elapsed)::period::den) / r.elapsed.count();
    Gbps_sum += Gbps;
  }

  auto const MiBs_avg = MiBs_sum / options->iteration_count;
  auto const Gbps_avg = Gbps_sum / options->iteration_count;
  std::cout << "# Average Bandwidth (MiB/s): " << MiBs_avg << "\n";
  std::cout << "# Average Bandwidth (Gbps): " << Gbps_avg << "\n";

  DeleteAllObjects(client, bucket_name, *options, object_names);

  std::cout << "# Deleting " << bucket_name << "\n";
  auto status = client.DeleteBucket(bucket_name);
  if (!status.ok()) {
    std::cerr << "# Error deleting bucket, status=" << status << "\n";
    return 1;
  }

  return 0;
}

namespace {

void CreateGroup(gcs::Client client, std::string const& bucket_name,
                 Options const& options, std::vector<std::string> group) {
  google::cloud::internal::DefaultPRNG generator =
      google::cloud::internal::MakeDefaultPRNG();

  std::string const random_data =
      gcs_bm::MakeRandomData(generator, options.chunk_size);
  for (auto const& object_name : group) {
    auto stream = client.WriteObject(bucket_name, object_name, gcs::Fields(""));
    for (std::int64_t count = 0; count != options.chunk_count; ++count) {
      stream.write(random_data.data(), random_data.size());
    }
    stream.Close();
    if (!stream.metadata()) {
      std::cerr << "Error writing: " << object_name << "\n";
    }
  }
}

std::vector<std::string> CreateAllObjects(
    gcs::Client client, google::cloud::internal::DefaultPRNG& gen,
    std::string const& bucket_name, Options const& options) {
  using std::chrono::duration_cast;
  using std::chrono::milliseconds;

  std::size_t const max_group_size =
      std::max(options.object_count / options.thread_count, 1);
  std::cout << "# Creating test objects [" << max_group_size << "]\n";

  // Generate the list of object names.
  std::vector<std::string> object_names;
  object_names.reserve(options.object_count);
  for (long c = 0; c != options.object_count; ++c) {
    object_names.emplace_back(gcs_bm::MakeRandomObjectName(gen));
  }

  // Split the objects in more or less equally sized groups, launch a thread
  // to create the objects in each group.
  auto start = std::chrono::steady_clock::now();
  std::vector<std::future<void>> tasks;
  std::vector<std::string> group;
  for (auto const& o : object_names) {
    group.push_back(o);
    if (group.size() >= max_group_size) {
      tasks.emplace_back(std::async(std::launch::async, &CreateGroup, client,
                                    bucket_name, options, std::move(group)));
      group = {};  // after a move, must assign to guarantee it is valid.
    }
  }
  if (!group.empty()) {
    tasks.emplace_back(std::async(std::launch::async, &CreateGroup, client,
                                  bucket_name, options, std::move(group)));
  }
  // Wait for the threads to finish.
  for (auto& t : tasks) {
    t.get();
  }
  auto elapsed = std::chrono::steady_clock::now() - start;
  std::cout << "# Created in " << duration_cast<milliseconds>(elapsed).count()
            << "ms\n";
  return object_names;
}

extern "C" std::size_t OnWrite(void*, std::size_t size, std::size_t nmemb,
                               void* userdata) {
  auto counter = reinterpret_cast<std::size_t*>(userdata);
  *counter += size * nmemb;
  return size * nmemb;
}

std::size_t WorkerThread(WorkItemQueue& work_queue) {
  std::size_t bytes_received = 0;
  auto client = gcs::Client::CreateDefaultClient();
  if (!client) return bytes_received;

  auto const client_options = client->raw_client()->client_options();
  auto authorization = client_options.credentials()->AuthorizationHeader();
  if (!authorization) return bytes_received;

  gcs::internal::CurlMulti multi(curl_multi_init(), &curl_multi_cleanup);
  gcs::internal::CurlShare share(curl_share_init(), &curl_share_cleanup);
  curl_share_setopt(share.get(), CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
  curl_share_setopt(share.get(), CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
  curl_share_setopt(share.get(), CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);

  for (auto w = work_queue.Pop(); w.has_value(); w = work_queue.Pop()) {
    std::vector<gcs::internal::CurlPtr> handles;
    std::vector<gcs::internal::CurlHeaders> headers;

    for (auto const& item : *w) {
      gcs::internal::CurlPtr handle(curl_easy_init(), &curl_easy_cleanup);
      gcs::internal::CurlHeaders request_headers(nullptr, &curl_slist_free_all);
      request_headers.reset(
          curl_slist_append(request_headers.release(), authorization->c_str()));
      request_headers.reset(curl_slist_append(
          request_headers.release(), "Host: storage-download.googleapis.com"));
      request_headers.reset(
          curl_slist_append(request_headers.release(),
                            ("Range: bytes=" + std::to_string(item.begin) +
                             "-" + std::to_string(item.end - 1))
                                .c_str()));

      // TODO(...) - this does not work with URL-unsafe object names.
      auto const url = "https://storage-download.googleapis.com/" +
                       item.bucket + "/" + item.object;

      curl_easy_setopt(handle.get(), CURLOPT_BUFFERSIZE, 128 * 1024L);
      curl_easy_setopt(handle.get(), CURLOPT_URL, url.c_str());
      curl_easy_setopt(handle.get(), CURLOPT_HTTPHEADER, request_headers.get());
      curl_easy_setopt(handle.get(), CURLOPT_USERAGENT,
                       client_options.user_agent_prefix().c_str());
      curl_easy_setopt(handle.get(), CURLOPT_TCP_KEEPALIVE, 1L);
      curl_easy_setopt(handle.get(), CURLOPT_WRITEFUNCTION, &OnWrite);
      curl_easy_setopt(handle.get(), CURLOPT_WRITEDATA, &bytes_received);
      curl_easy_setopt(handle.get(), CURLOPT_SHARE, share.get());

      curl_multi_add_handle(multi.get(), handle.get());
      handles.push_back(std::move(handle));
      headers.push_back(std::move(request_headers));
    }

    int running_handles;
    curl_multi_perform(multi.get(), &running_handles);
    while (running_handles != 0) {
      int numfds = 0;
      auto e = curl_multi_wait(multi.get(), nullptr, 0, 1000, &numfds);
      if (e != CURLM_OK) break;
      curl_multi_perform(multi.get(), &running_handles);
    }
    int remaining = 0;
    while (auto msg = curl_multi_info_read(multi.get(), &remaining)) {
      if (msg->data.result == CURLE_OK) continue;
      std::cout << "# Error in curl_multi_info_read(): "
                << " msg=" << msg->msg
                << " code=" << curl_easy_strerror(msg->data.result)
                << std::endl;
      curl_multi_remove_handle(multi.get(), msg->easy_handle);
    }
  }
  return bytes_received;
}

IterationResult RunOneIteration(google::cloud::internal::DefaultPRNG& generator,
                                Options const& options,
                                std::string const& bucket_name,
                                std::vector<std::string> const& object_names) {
  using std::chrono::duration_cast;
  using std::chrono::microseconds;

  WorkItemQueue work_queue;
  std::vector<std::future<std::size_t>> workers;
  std::generate_n(std::back_inserter(workers), options.thread_count,
                  [&work_queue] {
                    return std::async(std::launch::async, WorkerThread,
                                      std::ref(work_queue));
                  });

  std::uniform_int_distribution<std::size_t> object_generator(
      0, object_names.size() - 1);
  std::uniform_int_distribution<std::int64_t> chunk_generator(
      0, options.chunk_count - 1);

  auto const download_start = std::chrono::steady_clock::now();
  std::int64_t total_bytes_requested = 0;
  // No more than 512 total transfers at the same time (across all threads)
  auto const max_batch_size = std::size_t(256) / options.thread_count;
  for (int t = 0; t != options.thread_count; ++t) {
    // Distribute the batches across the threads.
    std::vector<WorkItem> batch;
    batch.reserve(max_batch_size);
    for (long i = 0; i != options.iteration_size; ++i) {
      if (i % options.thread_count != t) continue;
      auto const object = object_generator(generator);
      auto const chunk = chunk_generator(generator);
      batch.push_back({bucket_name, object_names.at(object),
                       chunk * options.chunk_size,
                       (chunk + 1) * options.chunk_size});
      total_bytes_requested += options.chunk_size;
      if (batch.size() > max_batch_size) {
        work_queue.Push(std::move(batch));
        batch = {};
      }
    }
    if (!batch.empty()) {
      work_queue.Push(std::move(batch));
    }
  }
  work_queue.Shutdown();
  std::int64_t total_bytes_received = 0;
  for (auto& t : workers) {
    total_bytes_received += t.get();
  }
  auto const elapsed = std::chrono::steady_clock::now() - download_start;
  return {total_bytes_requested, total_bytes_received,
          duration_cast<microseconds>(elapsed)};
}

google::cloud::Status DeleteGroup(gcs::Client client,
                                  std::vector<gcs::ObjectMetadata> group) {
  google::cloud::Status final_status{};
  for (auto const& o : group) {
    auto status = client.DeleteObject(o.bucket(), o.name(),
                                      gcs::Generation(o.generation()));
    if (!status.ok()) {
      final_status = std::move(status);
      continue;
    }
  }
  return final_status;
}

void DeleteAllObjects(gcs::Client client, std::string const& bucket_name,
                      Options const& options, std::vector<std::string> const&) {
  using std::chrono::duration_cast;
  using std::chrono::milliseconds;

  auto const max_group_size =
      std::max(options.object_count / options.thread_count, 1);

  std::cout << "# Deleting test objects [" << max_group_size << "]\n";
  auto start = std::chrono::steady_clock::now();
  std::vector<std::future<google::cloud::Status>> tasks;
  std::vector<gcs::ObjectMetadata> group;
  for (auto&& o : client.ListObjects(bucket_name, gcs::Versions(true))) {
    group.emplace_back(std::move(o).value());
    if (group.size() >= static_cast<std::size_t>(max_group_size)) {
      tasks.emplace_back(std::async(std::launch::async, &DeleteGroup, client,
                                    std::move(group)));
      group = {};  // after a move, must assign to guarantee it is valid.
    }
  }
  if (!group.empty()) {
    tasks.emplace_back(
        std::async(std::launch::async, &DeleteGroup, client, std::move(group)));
  }
  int count = 0;
  for (auto& t : tasks) {
    auto status = t.get();
    if (!status.ok()) {
      std::cerr << "Error return task[" << count << "]: " << status << "\n";
    }
    ++count;
  }
  // We do not print the latency to delete the objects because we have another
  // benchmark to measure that.
  auto elapsed = std::chrono::steady_clock::now() - start;
  std::cout << "# Deleted in " << duration_cast<milliseconds>(elapsed).count()
            << "ms\n";
}

google::cloud::StatusOr<Options> ParseArgs(int argc, char* argv[]) {
  Options options;
  bool wants_help = false;
  bool wants_description = false;
  std::vector<gcs_bm::OptionDescriptor> desc{
      {"--help", "print usage information",
       [&wants_help](std::string const&) { wants_help = true; }},
      {"--description", "print benchmark description",
       [&wants_description](std::string const&) { wants_description = true; }},
      {"--project-id", "use the given project id for the benchmark",
       [&options](std::string const& val) { options.project_id = val; }},
      {"--bucket-prefix", "configure the bucket's prefix",
       [&options](std::string const& val) { options.bucket_prefix = val; }},
      {"--region", "use the given region for the benchmark",
       [&options](std::string const& val) { options.region = val; }},
      {"--object-count", "set the number of objects created by the benchmark",
       [&options](std::string const& val) {
         options.object_count = std::stoi(val);
       }},
      {"--thread-count", "set the number of threads in the benchmark",
       [&options](std::string const& val) {
         options.thread_count = std::stoi(val);
       }},
      {"--iteration-size",
       "set the number of chunk downloaded in each iteration",
       [&options](std::string const& val) {
         options.iteration_size = std::stoi(val);
       }},
      {"--iteration-count",
       "set the number of samples captured by the benchmark",
       [&options](std::string const& val) {
         options.iteration_count = std::stoi(val);
       }},
      {"--chunk-size", "size of the chunks used in the benchmark",
       [&options](std::string const& val) {
         options.chunk_size = gcs_bm::ParseSize(val);
       }},
      {"--chunk-count", "the number of chunks in each object",
       [&options](std::string const& val) {
         options.chunk_count = std::stoi(val);
       }},
  };
  auto usage = gcs_bm::BuildUsage(desc, argv[0]);

  auto unparsed = gcs_bm::OptionsParse(desc, {argv, argv + argc});
  if (wants_help) {
    std::cout << usage << "\n";
  }

  if (wants_description) {
    std::cout << kDescription << "\n";
  }

  if (unparsed.size() > 2) {
    std::ostringstream os;
    os << "Unknown arguments or options\n" << usage << "\n";
    return google::cloud::Status{google::cloud::StatusCode::kInvalidArgument,
                                 std::move(os).str()};
  }
  if (unparsed.size() == 2) {
    options.region = unparsed[1];
  }
  if (options.region.empty()) {
    std::ostringstream os;
    os << "Missing value for --region option" << usage << "\n";
    return google::cloud::Status{google::cloud::StatusCode::kInvalidArgument,
                                 std::move(os).str()};
  }

  return options;
}

}  // namespace
