// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/internal/port_platform.h"
#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC && GOOGLE_CLOUD_CPP_HAVE_COROUTINES
#include "google/cloud/storage/async/client.h"
#include "google/cloud/storage/benchmarks/benchmark_utils.h"
#include "google/cloud/options.h"
#include "google/cloud/status.h"
#include "google/cloud/testing_util/command_line_parsing.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

auto constexpr kDescription = R"""(
This program benchmarks uploads to GCS (Google Cloud Storage) and downloads from
it using the google::cloud::storage_experimental::AsyncClient APIs.

The benchmark tries to answer the following questions:

1) How much CPU is used by the client library for uploads?
2) How much CPU is used by the client library for downloads?

The answer to these questions depend on many parameters, including the size of
the size of the upload/download, the size of the buffers used for streaming
operations, whether checksums are enabled, etc.

Incidentally this benchmark also provides answers to these questions:

3) What kind of throughput one can expect for uploads via AsyncClient?
4) What kind of throughput one can expect for downloads via AsyncClient?

But we should note that the throughput achieved depends on the VM configuration,
the location of the VM and the bucket, and may depend on the activity in GCS.

At a high level, the benchmark performs N concurrent uploads and measures the
CPU usage for the program.

The benchmark then performs N concurrent downloads and measures the CPU usage
for the program.
)""";

namespace gcs = google::cloud::storage;
namespace gcs_ex = google::cloud::storage_experimental;

using ::google::cloud::storage_benchmarks::kMiB;

struct Configuration {
  std::string labels;
  std::string bucket;
  int concurrency = 1;
  int iterations = 1;
  std::uint64_t object_size = 512 * kMiB;
  google::cloud::Options options;
};

namespace g = google::cloud;

struct Result {
  std::string object_name;
  std::int64_t generation;
  std::uint64_t object_size;
  std::chrono::nanoseconds elapsed;
};

std::string RandomObject(std::mt19937_64& gen) {
  auto const population = std::string_view(
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghiljkmnopqrstuvwxyz0123456789");
  auto result = std::string{};
  std::generate_n(std::back_inserter(result), 32, [&]() mutable {
    auto u =
        std::uniform_int_distribution<std::size_t>(0, population.size() - 1);
    return population[u(gen)];
  });
  return result;
}

std::string RandomData(std::mt19937_64& gen) {
  auto const population = std::string_view(
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghiljkmnopqrstuvwxyz0123456789");
  auto result = std::string{};
  std::generate_n(std::back_inserter(result), 1024 * 1024L,
                  [&, i = 0L]() mutable {
                    if (i++ % 64 == 0) return '\n';
                    auto u = std::uniform_int_distribution<std::size_t>(
                        0, population.size() - 1);
                    return population[u(gen)];
                  });
  return result;
}

g::future<Result> BenchmarkWrite(gcs_ex::AsyncClient client,
                                 Configuration const& config,
                                 std::string const& object_name,
                                 std::string const& data) {
  auto const start = std::chrono::steady_clock::now();
  auto [writer, token] =
      (co_await client.WriteObject(config.bucket, object_name)).value();

  using google::cloud::storage_experimental::WritePayload;
  for (auto remaining = config.object_size; remaining != 0;) {
    if (!token.valid()) break;
    auto const n = std::min(static_cast<std::uint64_t>(data.size()), remaining);
    remaining -= n;
    // This copy is intentional. The benchmark is more realistic if we assume
    // the source data has to be copied into the payload.
    auto payload = WritePayload(data.substr(0, static_cast<std::size_t>(n)));
    token =
        (co_await writer.Write(std::move(token), std::move(payload))).value();
  }

  auto metadata = (co_await writer.Finalize(std::move(token))).value();
  auto const elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now() - start);
  co_return Result{object_name, metadata.generation(), metadata.size(),
                   elapsed};
}

g::future<Result> BenchmarkRead(gcs_ex::AsyncClient client,
                                Configuration const& config,
                                std::string const& object_name) {
  auto const start = std::chrono::steady_clock::now();
  auto [reader, token] =
      (co_await client.ReadObject(config.bucket, object_name)).value();

  std::int64_t generation = 0;
  std::uint64_t size = 0;
  while (token.valid()) {
    auto [response, t] = (co_await reader.Read(std::move(token))).value();
    token = std::move(t);
    if (response.metadata().has_value()) {
      generation = response.metadata()->generation();
    }
    size += response.size();
  }

  auto const elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now() - start);
  co_return Result{object_name, generation, size, elapsed};
}

g::future<Result> BenchmarkDelete(gcs_ex::AsyncClient client,
                                  Configuration const& config,
                                  std::string const& object_name,
                                  std::int64_t generation) {
  auto const start = std::chrono::steady_clock::now();
  auto status = co_await client.DeleteObject(config.bucket, object_name,
                                             gcs::Generation(generation));
  auto const elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now() - start);
  co_return Result{object_name, generation, 0, elapsed};
}

using ResultHandler =
    std::function<void(std::chrono::system_clock::time_point start,
                       std::string_view name, Result const& result)>;

void Worker(gcs_ex::AsyncClient const& client, Configuration const& config,
            ResultHandler const& handler) {
  std::mt19937_64 gen(std::random_device{}());
  auto const data = RandomData(gen);
  for (int i = 0; i != config.iterations; ++i) {
    auto const worker_start = std::chrono::system_clock::now();
    auto const object_name = RandomObject(gen);
    auto w = BenchmarkWrite(client, config, object_name, data).get();
    auto r1 = BenchmarkRead(client, config, object_name).get();
    auto r2 = BenchmarkRead(client, config, object_name).get();
    auto r3 = BenchmarkRead(client, config, object_name).get();
    auto d = BenchmarkDelete(client, config, object_name, w.generation).get();
    handler(worker_start, "WRITE", w);
    handler(worker_start, "READ[1]", r1);
    handler(worker_start, "READ[2]", r2);
    handler(worker_start, "READ[3]", r3);
    handler(worker_start, "DELETE", d);
  }
}

void RunBenchmark(Configuration const& config) {
  using ::google::cloud::storage_benchmarks::FormatBandwidthGbPerSecond;
  using ::google::cloud::storage_benchmarks::FormatBandwidthMiBPerSecond;
  using ::google::cloud::storage_benchmarks::FormatTimestamp;
  std::mutex mu;
  auto handler = [&mu](std::chrono::system_clock::time_point start,
                       std::string_view name, Result const& r) {
    std::lock_guard lk(mu);
    auto gigabit_per_second =
        FormatBandwidthGbPerSecond(r.object_size, r.elapsed);
    auto const mebibyte_per_second =
        FormatBandwidthMiBPerSecond(r.object_size, r.elapsed);
    std::cout << FormatTimestamp(start)      //
              << ',' << name                 //
              << ',' << r.object_size        //
              << ',' << r.elapsed.count()    //
              << ',' << r.object_name        //
              << ',' << r.generation         //
              << ',' << gigabit_per_second   //
              << ',' << mebibyte_per_second  //
              << "\n";
  };

  auto client = gcs_ex::AsyncClient(config.options);
  auto make_worker = [&] {
    return std::async(std::launch::async, Worker, client, config, handler);
  };

  std::cout << "# START\n";
  std::cout << "Start,Operation,ObjectSize,ElapsedNanos,ObjectName,"
               "Generation,Gbps,MiBs\n";
  std::vector<std::future<void>> workers(config.concurrency);
  std::generate(workers.begin(), workers.end(), make_worker);
  for (auto& t : workers) t.get();
  std::cout << "# DONE\n";
}

Configuration ParseArgs(std::vector<std::string> argv) {
  using ::google::cloud::storage_benchmarks::ParseSize;
  using ::google::cloud::testing_util::OptionDescriptor;

  bool help = false;
  bool description = false;
  Configuration config;
  std::vector<OptionDescriptor> desc{
      {"--help", "print usage information",
       [&help](std::string const&) { help = true; }},
      {"--description", "print benchmark description",
       [&description](std::string const&) { description = true; }},
      {"--bucket", "select the bucket for the benchmark",
       [&config](std::string const& v) { config.bucket = v; }},
      {"--iterations", "select the number of iterations in the benchmark",
       [&config](std::string const& v) { config.iterations = std::stoi(v); }},
      {"--object-size", "select the object size",
       [&config](std::string const& v) { config.object_size = ParseSize(v); }},
      {"--grpc-endpoint", "select the endpoint for gRPC",
       [&config](std::string const& v) {
         config.options.set<g::EndpointOption>(v);
       }},
  };
  auto usage = google::cloud::testing_util::BuildUsage(desc, argv[0]);
  google::cloud::testing_util::OptionsParse(desc, std::move(argv));
  if (help) {
    std::cerr << usage << "\n";
    std::exit(0);
  }
  if (description) {
    std::cerr << kDescription << "\n";
    std::exit(0);
  }
  if (config.bucket.empty()) {
    throw std::invalid_argument("empty value for --bucket option");
  }
  return config;
}

}  // namespace

int main(int argc, char* argv[]) try {
  RunBenchmark(ParseArgs({argv, argv + argc}));
  return 0;
} catch (google::cloud::Status const& ex) {
  std::cerr << "Status error caught " << ex << "\n";
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception caught " << ex.what() << "\n";
  return 1;
}

#else
#include <iostream>

int main() {
  std::cout << "The storage_experimental::AsyncClient benchmarks require"
            << " C++20 coroutines and the GCS+gRPC plugin\n";
  return 0;
}

#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC &&
        // GOOGLE_CLOUD_CPP_HAVE_COROUTINES
