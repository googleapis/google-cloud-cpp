// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// MODIFIED: This benchmark has been refactored to test ListObjects pagination
// strategies (pageToken vs. startOffset) instead of upload/download throughput.

#include "google/cloud/storage/benchmarks/benchmark_utils.h"
#include "google/cloud/storage/benchmarks/throughput_experiment.h"
#include "google/cloud/storage/benchmarks/throughput_options.h"
#include "google/cloud/storage/object_metadata.h"
#include "google/cloud/storage/benchmarks/throughput_result.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/build_info.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/log.h"
#include "absl/time/time.h"
#include <algorithm>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {
namespace gcs = ::google::cloud::storage;
namespace gcs_bm = ::google::cloud::storage_benchmarks;
using gcs_bm::ExperimentLibrary;
using gcs_bm::ExperimentTransport;
using gcs_bm::ThroughputOptions;

// MODIFIED: This description is updated for the new benchmark
char const kDescription[] = R"""(
A benchmark for GCS ListObjects pagination strategies.

This program measures the performance (wall time) of listing objects in a
bucket prefix using two different pagination strategies:

1.  `pageToken`: The efficient, stateful pagination method using the
    `nextPageToken` provided by the API.
2.  `startOffset`: The inefficient, stateless method that simulates pagination
    by restarting the list from a given object name (`startOffset`).

The program runs one "experiment" (a full scan of a prefix up to a
maximum number of pages) multiple times. Each *individual page fetch*
is timed and reported as a separate sample in the output CSV.

This allows you to plot "page number" vs. "latency" to
compare the performance degradation of `startOffset` against the
constant-time performance of `pageToken`.

Command-line arguments have been re-purposed:
  --minimum-object-size = The number of items to fetch per page (e.g., 1000).
  --maximum-object-size = The maximum number of pages to fetch in one run.

New arguments have been added:
  --prefix=...         (Required) The object prefix to list (e.g., "my-prefix/").
  --strategy=...       (Required) The pagination strategy:
                       "page-token" or "start-offset".
)""";

using ResultHandler =
    std::function<void(ThroughputOptions const&, gcs_bm::ThroughputResult)>;

gcs_bm::ClientProvider MakeProvider(ThroughputOptions const& options);

// MODIFIED: Added prefix, strategy, and print_mutex
void RunThread(ThroughputOptions const& ThroughputOptions, int thread_id,
               ResultHandler const& handler,
               gcs_bm::ClientProvider const& provider,
               std::string const& prefix, std::string const& strategy,
               std::mutex& print_mutex);

// MODIFIED: Simple struct to hold our new list options
struct ListOptions {
  std::string prefix;
  std::string strategy;
};

// MODIFIED: This function extracts our new custom arguments
ListOptions ParseListOptions(int& argc, char* argv[]) {
  ListOptions list_options;
  std::vector<char*> remaining_args;
  remaining_args.push_back(argv[0]);
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg.rfind("--prefix=", 0) == 0) {
      list_options.prefix = arg.substr(sizeof("--prefix=") - 1);
    } else if (arg.rfind("--strategy=", 0) == 0) {
      list_options.strategy = arg.substr(sizeof("--strategy=") - 1);
    } else {
      remaining_args.push_back(argv[i]);
    }
  }
  // Update argc and argv to remove the arguments we just parsed
  argc = static_cast<int>(remaining_args.size());
  for (std::size_t i = 0; i < remaining_args.size(); ++i) {
    argv[i] = remaining_args[i];
  }
  return list_options;
}

// Parses a new --create-objects=N argument
std::pair<int, std::string> ParseCreateOptions(int& argc, char* argv[]) {
  // int count = 500000;
  // std::string prefix ="gcs-cpp-benchmark-prefix-1/";
  int count = 0;
  std::string prefix ="";
  return {count, prefix};
}

// The worker function for a single thread
void UploadWorker(gcs::Client client, std::string bucket_name,
                  std::string prefix, int start_index, int end_index,
                  int thread_id) {
  for (int i = start_index; i < end_index; ++i) {
    std::ostringstream name_stream;
    name_stream << prefix << "object-" << std::setw(10) << std::setfill('0')
                << i;
    std::string object_name = name_stream.str();
    std::string content = "This is test object " + object_name;

    auto status =
        client.InsertObject(bucket_name, object_name, std::move(content),
                            gcs::IfGenerationMatch(0));

    if (!status && status.status().code() != google::cloud::StatusCode::kFailedPrecondition) {
      std::cerr << "[Thread " << thread_id << "] Failed to upload "
                << object_name << ": " << status.status() << "\n";
    }

    if (i % 1000 == 0 && i != start_index) {
      std::cout << "[Thread " << thread_id << "] ... uploaded " << object_name
                << "\n";
    }
  }
  std::cout << "[Thread " << thread_id << "] finished batch " << start_index
            << " - " << end_index << "\n";
}

// Manages parallel upload of N objects
void CreateObjects(gcs_bm::ClientProvider const& provider,
                   std::string bucket_name, std::string prefix,
                   int object_count, int thread_count) {
  auto transport = gcs_bm::ExperimentTransport::kGrpc;
  std::vector<std::future<void>> tasks;
  int batch_size = (object_count + thread_count - 1) / thread_count;
  for (int i = 0; i < thread_count; ++i) {
    int start_index = i * batch_size;
    int end_index = (std::min)((i + 1) * batch_size, object_count);
    
    if (start_index >= end_index) continue;

    tasks.emplace_back(std::async(std::launch::async, UploadWorker,
                                  provider(transport), bucket_name, prefix,
                                  start_index, end_index, i));
  }
  for (auto& f : tasks) {
    f.get();
  }
}

// THIS IS THE NEW, CORRECT VERSION
google::cloud::StatusOr<ThroughputOptions> SelfTest(
    char const* argv0, ListOptions& list_options);

google::cloud::StatusOr<ThroughputOptions> ParseArgs(int argc, char* argv[],
                                                   ListOptions& list_options) {
  list_options = ParseListOptions(argc, argv);

  bool auto_run =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES")
          .value_or("") == "yes";
  
  // FIX: If auto_run is set OR no arguments are provided (argc == 1),
  // run the SelfTest to get default values.
  if (auto_run || argc == 1) return SelfTest(argv[0], list_options);

  auto options =
      gcs_bm::ParseThroughputOptions({argv, argv + argc}, kDescription);
  if (!options) return options;

  // MODIFIED: Validate our custom arguments
  if (list_options.prefix.empty()) {
    return google::cloud::Status(
        google::cloud::StatusCode::kInvalidArgument,
        "Missing required argument: --prefix=<object-prefix>");
  }
  if (list_options.strategy != "page-token" &&
      list_options.strategy != "start-offset") {
    return google::cloud::Status(
        google::cloud::StatusCode::kInvalidArgument,
        "Invalid argument: --strategy must be 'page-token' or 'start-offset'");
  }

  options->labels = gcs_bm::AddDefaultLabels(std::move(options->labels));
  options->bucket = "vaibhav-test-001";
  return options;
}

}  // namespace

int main(int argc, char* argv[]) {
  // FIX: This line calls ParseCreateOptions, which fixes the warning
  int create_count = 0;
  std::string create_prefix;
  std::tie(create_count, create_prefix) = ParseCreateOptions(argc, argv);

  // Parse the rest of the arguments
  ListOptions list_options;
  google::cloud::StatusOr<ThroughputOptions> options =
      ParseArgs(argc, argv, list_options);
  if (!options) {
    std::cerr << options.status() << "\n";
    return 1;
  }
  if (options->exit_after_parse) return 0;
  options->bucket = "vaibhav-test-001";

  // --- THIS IS THE "EITHER/OR" LOGIC ---
  if (create_count > 0) {
    // MODE 1: Create objects and exit
    if (create_prefix.empty()) {
      create_prefix = list_options.prefix;
    }
    if (create_prefix.empty()) {
      std::cerr << "Error: --prefix is required when using --create-objects\n";
      return 1;
    }
    std::cout << "Starting object creation: " << create_count
              << " objects with prefix " << create_prefix << " in bucket "
              << options->bucket << " using " << options->thread_count
              << " threads...\n";
    auto provider = MakeProvider(*options);
    
    // FIX: This line calls CreateObjects, which fixes the warning
    CreateObjects(provider, options->bucket, create_prefix, create_count,
                  options->thread_count);
    std::cout << "Object creation complete.\n";
    
    // Exit successfully after creating objects
    return 0;
  }
  // --- END OF "EITHER/OR" LOGIC ---

  // MODE 2: Run the benchmark (this code is skipped if create_count > 0)
  std::string notes = google::cloud::storage::version_string() + ";" +
                      google::cloud::internal::compiler() + ";" +
                      google::cloud::internal::compiler_flags();
  std::transform(notes.begin(), notes.end(), notes.begin(),
                 [](char c) { return c == '\n' ? ';' : c; });

  struct Formatter {
    void operator()(std::string* out, ExperimentLibrary v) const {
      out->append(gcs_bm::ToString(v));
    }
    void operator()(std::string* out, ExperimentTransport v) const {
      out->append(gcs_bm::ToString(v));
    }
    void operator()(std::string* out, bool b) const {
      out->append(b ? "true" : "false");
    }
  };

  std::cout << "# Start time: " << gcs_bm::CurrentTime()          //
            << "\n# Labels: " << options->labels                  //
            << "\n# Running test on bucket: " << options->bucket  //
            << "\n# Strategy: " << list_options.strategy           //
            << "\n# Prefix: " << list_options.prefix               //
            << "\n# Page Size (from --maximum-object-size): "    //
            << options->minimum_object_size
            << "\n# Max Pages (from --minimum-object-size): "  //
            << options->maximum_object_size
            << "\n# Duration: "
            << absl::FormatDuration(absl::FromChrono(options->duration))
            << "\n# Thread Count: " << options->thread_count
            << "\n# Client Per Thread: " << options->client_per_thread;

  std::cout << "\n# Enabled Libs: "
            << absl::StrJoin(options->libs, ",", Formatter{})
            << "\n# Enabled Transports: "
            << absl::StrJoin(options->transports, ",", Formatter{})
            << "\n# Minimum Sample Count: " << options->minimum_sample_count
            << "\n# Maximum Sample Count: " << options->maximum_sample_count;

  gcs_bm::PrintOptions(std::cout, "Common", options->client_options);
  gcs_bm::PrintOptions(std::cout, "Json", options->rest_options);
  gcs_bm::PrintOptions(std::cout, "Grpc", options->grpc_options);
  gcs_bm::PrintOptions(std::cout, "Direct Path", options->direct_path_options);

  std::cout << "\n# Build info: " << notes << "\n";

  // --- MODIFICATION ---
  // Print a header for our clean, machine-readable data
  std::cout << "# New CSV Data Header:\n"
            << "DATA_ROW_HEADER,Transport,Strategy,TotalObjects,PageSize,"
               "LatencySeconds\n";
  // --- END MODIFICATION ---

  std::cout << std::flush;

  std::chrono::microseconds total_latency{0};

  std::mutex mu;
  auto handler = [&mu, &total_latency](ThroughputOptions const& options,
                       gcs_bm::ThroughputResult const& result) {
    std::lock_guard<std::mutex> lk(mu);
    
    // ADD THIS LINE to accumulate the latency
    total_latency += result.elapsed_time;

    // This is commented out, as requested.
    // gcs_bm::PrintAsCsv(std::cout, options, result);

    if (!result.status.ok()) {
      google::cloud::LogSink::Instance().Flush();
    }
  };
  auto provider = MakeProvider(*options);

  // Add a new mutex for printing sample summaries
  std::mutex print_mu;

  gcs_bm::PrintThroughputResultHeader(std::cout);
  std::vector<std::future<void>> tasks;
  for (int i = 0; i != options->thread_count; ++i) {
    // Pass the new print_mu by reference to each thread
    tasks.emplace_back(std::async(std::launch::async, RunThread, *options, i,
                                  handler, provider, list_options.prefix,
                                  list_options.strategy, std::ref(print_mu)));
  }
  for (auto& f : tasks) f.get();

  auto const total_seconds =
      std::chrono::duration_cast<std::chrono::duration<double>>(total_latency);
  std::cout << "#\n# Total Latency (sum of all page fetches): "
            << total_seconds.count() << " s\n";
  std::cout << "# DONE\n" << std::flush;

  return 1;
}

namespace {

gcs_bm::ClientProvider PerTransport(gcs_bm::ClientProvider const& provider) {
  struct State {
    std::mutex mu;
    std::map<ExperimentTransport, gcs::Client> clients;
  };
  auto state = std::make_shared<State>();
  return [state, provider](ExperimentTransport t) mutable {
    std::lock_guard<std::mutex> lk{state->mu};
    auto l = state->clients.find(t);
    if (l != state->clients.end()) return l->second;
    auto p = state->clients.emplace(t, provider(t));
    return p.first->second;
  };
}

gcs_bm::ClientProvider BaseProvider(ThroughputOptions const& options) {
  return [=](ExperimentTransport t) {
    auto opts = options.client_options;
#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
    if (t == ExperimentTransport::kDirectPath) {
      opts = google::cloud::internal::MergeOptions(options.direct_path_options,
                                                   std::move(opts));
      // std::cout << "direct path\n"; // Quieting this for clean output
      return gcs::MakeGrpcClient(std::move(opts));
    }
    if (t == ExperimentTransport::kGrpc) {
      opts = google::cloud::internal::MergeOptions(options.grpc_options,
                                                   std::move(opts));
      // std::cout << "gRPC path\n"; // Quieting this for clean output
      return gcs::MakeGrpcClient(std::move(opts));
    }
#else
    (void)t;  // disable unused parameter warning
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
    // std::cout << "json path\n"; // Quieting this for clean output
    opts = google::cloud::internal::MergeOptions(options.rest_options,
                                                 std::move(opts));
    return gcs::Client(std::move(opts));
  };
}

gcs_bm::ClientProvider MakeProvider(ThroughputOptions const& options) {
  auto provider = BaseProvider(options);
  if (!options.client_per_thread) provider = PerTransport(std::move(provider));
  return provider;
}

// MODIFIED: This is the new benchmark function for a single experiment run
// (i.e., one full pagination loop). It calls the handler for *each page*.
void RunListBenchmark(ThroughputOptions const& options,
                      ResultHandler const& handler, gcs::Client client,
                      std::string const& prefix,
                      std::string const& strategy) {
  // Re-purposed options
  std::int64_t const page_size = options.minimum_object_size;
  std::int64_t const max_pages = options.maximum_object_size;
  int page_number = 0;

  // Use the first lib/transport configured for reporting
  auto const lib = options.libs[0];
  auto const transport = options.transports[0];
  

  if (strategy == "page-token") {
    // The 'page-token' strategy.
    // We cannot access 'pages' directly. Instead, we iterate the object
    // stream (which uses pageToken efficiently) and 'chunk' the results
    // ourselves, timing each chunk to simulate a page fetch.
    auto reader = client.ListObjects(options.bucket, gcs::Prefix(prefix));
    auto object_it = reader.begin();
    bool done = false;

    do {
      page_number++;
      auto const system_start = std::chrono::system_clock::now();
      auto const steady_start = std::chrono::steady_clock::now();

      std::size_t items_in_this_page = 0;
      google::cloud::Status status;

      // Pull 'page_size' items from the stream
      while (items_in_this_page < static_cast<std::size_t>(page_size)) {
        if (object_it == reader.end()) {
          done = true;
          break;
        }
        auto& object = *object_it;
        if (!object) {
          status = std::move(object).status();
          done = true;
          break;
        }
        status = google::cloud::Status(); // We got an item, so it's OK
        items_in_this_page++;
        ++object_it;
      }
      
      auto const steady_end = std::chrono::steady_clock::now();
      auto const elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
          steady_end - steady_start);

      // Report this chunk as one sample
      handler(options, gcs_bm::ThroughputResult{
                           system_start,
                           lib,
                           transport,
                           gcs_bm::kOpRead0,
                           static_cast<std::int64_t>(items_in_this_page),
                           static_cast<std::int64_t>(page_number),
                           0,      // read_offset
                           false,  // crc_enabled
                           false,  // md5_enabled
                           // FIX: Add the missing 3rd boolean argument
                           false,
                           elapsed,
                           std::chrono::microseconds(0),  // No CPU time
                           status});

      if (done || page_number >= max_pages) break;
    } while (true);

  } else {  // "start-offset"
    std::string next_start_offset;
    do {
      page_number++;
      auto const system_start = std::chrono::system_clock::now();
      auto const steady_start = std::chrono::steady_clock::now();

      // This logic matches your pseudocode: create a new reader on each
      // iteration
      auto reader = client.ListObjects(
          options.bucket, gcs::Prefix(prefix),
          gcs::StartOffset(next_start_offset), gcs::MaxResults(page_size));

      std::vector<gcs::ObjectMetadata> items;
      google::cloud::Status status;

      // Iterate just enough to fill one page
      for (auto& object : reader) {
        if (!object) {
          status = std::move(object).status();
          break;
        }
        status = google::cloud::Status();
        // Skip the first object if it's the one we started from
        if (!next_start_offset.empty() &&
            object->name() == next_start_offset) {
          continue;
        }
        items.push_back(std::move(*object));
        if (items.size() >= static_cast<std::size_t>(page_size)) break;
      }

      auto const steady_end = std::chrono::steady_clock::now();
      auto const elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
          steady_end - steady_start);

      if (status.ok() && !items.empty()) {
        next_start_offset = items.back().name();
      } else if (status.ok() && items.empty()) {
        // No more items, stop the loop
        next_start_offset.clear();
        break;
      }

      // Report this page fetch as one sample
      handler(options, gcs_bm::ThroughputResult{
                           system_start,
                           lib,
                           transport,
                           gcs_bm::kOpRead1,
                           static_cast<std::int64_t>(items.size()),
                           static_cast<std::int64_t>(page_number),
                           0,      // read_offset
                           false,  // crc_enabled
                           false,  // md5_enabled
                           // FIX: Add the missing 3rd boolean argument
                           false,
                           elapsed,
                           std::chrono::microseconds(0),  // No CPU time
                           status});

      if (!status.ok()) break;             // Error
      if (items.empty()) break;            // End of list
      if (next_start_offset.empty()) break; // End of list
    } while (page_number < max_pages);
  }
}


// MODIFIED: This function is completely refactored for the list benchmark
// Add std::mutex& print_mutex and restore thread_id parameter
void RunThread(ThroughputOptions const& options, int thread_id,
               ResultHandler const& handler,
               gcs_bm::ClientProvider const& provider,
               std::string const& prefix, std::string const& strategy,
               std::mutex& print_mutex) {

  // Each thread gets its own client(s)
  auto client = provider(options.transports[0]);

  auto deadline = std::chrono::steady_clock::now() + options.duration;

  std::int32_t iteration_count = 0;
  for (auto start = std::chrono::steady_clock::now();
       iteration_count < options.maximum_sample_count &&
       (iteration_count < options.minimum_sample_count || start < deadline);
       start = std::chrono::steady_clock::now(), ++iteration_count) {
    
    // Time the *entire* RunListBenchmark call
    auto const sample_start_time = std::chrono::steady_clock::now();

    // Run one full experiment (one full prefix scan)
    RunListBenchmark(options, handler, client, prefix, strategy);

    auto const sample_end_time = std::chrono::steady_clock::now();
    auto const sample_latency =
        sample_end_time - sample_start_time;
    
    auto const sample_seconds =
      std::chrono::duration_cast<std::chrono::duration<double>>(sample_latency);

    // --- MODIFICATION ---
    // Calculate the "Total Objects" based on the run's parameters
    std::int64_t const page_size = options.minimum_object_size;
    std::int64_t const max_pages = options.maximum_object_size;
    std::int64_t total_objects = page_size * max_pages;
    
    // Use the mutex to print the summary for this sample
    {
      std::lock_guard<std::mutex> lk(print_mutex);
      
      // This is the human-readable line (commented out for clean CSV)
      // std::cout << "# [Thread " << thread_id << ", Sample " << iteration_count + 1
      //           << "] Total latency for this sample: "
      //           << sample_seconds.count() << " s\n";

      // This is the clean CSV line we will capture
      // Format: DATA_ROW,Transport,Strategy,TotalObjects,PageSize,LatencySeconds
      std::cout << "DATA_ROW," 
                << gcs_bm::ToString(options.transports[0]) << ","
                << strategy << ","
                << total_objects << ","
                << page_size << ","
                << sample_seconds.count() << "\n";
    }
    // --- END MODIFICATION ---

    // If needed, pace the benchmark so each thread generates only so many
    // "experiments" (full scans) each second.
    auto const pace = start + options.minimum_sample_delay;
    auto const now = std::chrono::steady_clock::now();
    if (pace > now) std::this_thread::sleep_for(pace - now);
  }
}



// MODIFIED: Updated SelfTest with new/re-purposed defaults
google::cloud::StatusOr<ThroughputOptions> SelfTest(
    char const* argv0, ListOptions& list_options) {
  using ::google::cloud::internal::GetEnv;
  using ::google::cloud::internal::Sample;

  std::string bucket_name = "vaibhav-test-001";
  if (bucket_name.empty()) {
    std::ostringstream os;
    os << "The GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME environment variable "
          "is not set or empty";
    return google::cloud::internal::UnknownError(std::move(os).str(),
                                                 GCP_ERROR_INFO());
  }

  // FIX: Set the default prefix and strategy on the 'list_options'
  // struct that was passed in by reference.
  if (list_options.prefix.empty()) {
    list_options.prefix = "gcs-cpp-benchmark-prefix/";
  }
  if (list_options.strategy.empty()) {
    list_options.strategy = "start-offset";
    // list_options.strategy = "page-token";
  }

  // FIX: This vector ONLY contains arguments that the original
  // ParseThroughputOptions function understands.
  // --prefix and --strategy have been REMOVED.
  std::vector<std::string> args = {
      argv0,
      "--bucket=vaibhav-test-001",
      "--thread-count=1",
      // Re-purposed: 1000 items per page
      "--minimum-object-size=1000",
      // Re-purposed: Fetch a max of 100 pages
      "--maximum-object-size=10000",
      // Remove all irrelevant args
      "--duration=30000s",
      "--minimum-sample-count=1",
      "--maximum-sample-count=10",
      "--enabled-transports=Grpc", // Use Grpc or Rest
  };

  return gcs_bm::ParseThroughputOptions(args, kDescription);
}

}  // namespace