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
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/build_info.h"
#include "google/cloud/options.h"
#include "google/cloud/status.h"
#include "google/cloud/testing_util/command_line_parsing.h"
#include "absl/strings/str_split.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace {

auto constexpr kDescription = R"""(
This program benchmarks concurrent uploads to and downloads from Google Cloud
Storage (GCS) using the `google::cloud::storage_experimental::AsyncClient APIs.

The benchmark tries to answer the following questions:

1) What kind of throughput can we expect when performing N uploads concurrently?
2) What kind of throughput can we expect when performing N downloads
   concurrently?
3) How is the throughput affected by the object size, the value of N, and the
   number of background threads?
4) How do the different `storage*Client` classes compare w.r.t. throughput or
   CPU usage.
5) Can we saturate the GCE VM bandwidth for uploads and/or downloads?
6) How much CPU is required to saturate the GCE VM bandwidth?

Customers often have questions similar to (1) or (2). While we cannot offer
guarantees around this, it is useful to have some guidance or at least a
starting program they can run.

We (the Cloud C++ team) are interested in questions like (4) or (5). Ideally the
client library is able to saturate the available I/O, without having to use all
the available CPU and/or RAM to do so.

The benchmark has many benchmark parameters, such as the number of objects, the
number of background threads, the size of the objects, etc. Sometimes choosing
different values for these parameters affects performance in non-obvious ways.

The program can be configured to randomly select these parameters before each
iteration. This can be useful when exploring how different values of these
parameters affect throughput.

At times it may be useful to use fixed ranges for some of these values. For
example, when trying to saturate the I/O one may want to use a single
`storage*Client`, transport and network path. Or one may want to compare the
benchmark results against the implementation in other languages.

Examples:

1) Try to saturage the egress path on the VM using back-to-back batches of
   concurrent uploads:

${program} --bucket=${BUCKET} \
    --minimum-background-threads=$(nproc) \
    --maximum-background-threads=$(nproc) \
    --minimum-concurrency=$(nproc) --maximum-concurrency=$(nproc) \
    --minimum-object-size=100MiB --maximum-object-size=100MiB \
    --minimum-object-count=100 --maximum-object-count=100 \
    --minimum-write-count=100 --maximum-read-count=100 \
    --minimum-read-count=0 --maximum-read-count=0

1.1) Same thing, but only use the AsyncClient:

${program} --bucket=${BUCKET} \
    --minimum-background-threads=$(nproc) \
    --maximum-background-threads=$(nproc) \
    --minimum-concurrency=$(nproc) --maximum-concurrency=$(nproc) \
    --minimum-object-size=100MiB --maximum-object-size=100MiB \
    --minimum-object-count=100 --maximum-object-count=100 \
    --minimum-write-count=100 --maximum-write-count=100 \
    --minimum-read-count=0 --maximum-read-count=0 \
    --clients=AsyncClient

2) Try to saturate the VM ingress path using back-to-back batches of concurrent
   downloads. Note the initial write to bootstrap the data set.

${program} --bucket=${BUCKET} \
    --minimum-background-threads=$(nproc) \
    --maximum-background-threads=$(nproc) \
    --minimum-concurrency=$(nproc) --maximum-concurrency=$(nproc) \
    --minimum-object-size=100MiB --maximum-object-size=100MiB \
    --minimum-object-count=100 --maximum-object-count=100 \
    --minimum-write-count=1 --maximum-write-count=1 \
    --minimum-read-count=100 --maximum-read-count=100

2.1) Same thing, but only use the AsyncClient:

${program} --bucket=${BUCKET} \
    --minimum-background-threads=$(nproc) \
    --maximum-background-threads=$(nproc) \
    --minimum-object-size=100MiB --maximum-object-size=100MiB \
    --minimum-object-count=100 --maximum-object-count=100 \
    --minimum-write-count=1 --maximum-read-count=1 \
    --minimum-read-count=100 --maximum-read-count=100 \
    --clients=AsyncClient

3) Generate data to compare single-stream throughput for different clients
   across a range of object sizes:

${program} --bucket=${BUCKET} \
    --minimum-object-size=0MiB --maximum-object-size=512MiB \
    --minimum-object-count=1 --maximum-object-count=1 \
    --minimum-write-count=1 --maximum-write-count=1 \
    --minimum-read-count=1 --maximum-read-count=1 \
    --iterations=1000

4) Generate data to compare single-stream latency for different clients for
   100KiB-sized objects:

${program} --bucket=${BUCKET} \
    --minimum-object-size=100KiB --maximum-object-size=100KiB \
    --minimum-object-count=1 --maximum-object-count=1 \
    --minimum-write-count=1 --maximum-write-count=1 \
    --minimum-read-count=1 --maximum-read-count=1 \
    --iterations=1000

5) Generate data to compare aggregated throughput for datasets of 100 objects
   each 100MB in size:

${program} --bucket=${BUCKET} \
    --minimum-background-threads=$(nproc) \
    --maximum-background-threads=$(nproc) \
    --concurrency=100 \
    --minimum-object-size=100MB --maximum-object-size=100MB \
    --minimum-object-count=100 --maximum-object-count=100 \
    --minimum-write-count=1 --maximum-write-count=1 \
    --minimum-read-count=1 --maximum-read-count=1 \
    --iterations=1000
)""";

namespace g = google::cloud;
namespace gcs = google::cloud::storage;
namespace gcs_ex = google::cloud::storage_experimental;

using ::google::cloud::storage_benchmarks::kMiB;

auto constexpr kAsyncClientName = "AsyncClient";
auto constexpr kSyncClientName = "SyncClient";
auto constexpr kJson = "JSON";
auto constexpr kGrpc = "GRPC";
auto constexpr kMissingPeer = "missing-peer";
auto constexpr kMissingUploadId = "missing-upload-id";

struct Configuration {
  std::string labels;
  std::string bucket;
  int iterations = 1;
  int minimum_object_count = 10;
  int maximum_object_count = 10;
  std::uint64_t minimum_object_size = 0;
  std::uint64_t maximum_object_size = 512 * kMiB;
  std::set<std::string> transports = {kGrpc, kJson};
  std::set<std::string> paths = {"CP", "DP"};
  std::set<std::string> clients = {kAsyncClientName, kSyncClientName};
  int minimum_write_count = 1;
  int maximum_write_count = 1;
  int minimum_read_count = 1;
  int maximum_read_count = 1;
  std::size_t chunk_size = 32 * kMiB;

  int minimum_concurrency = 1;
  int maximum_concurrency = 1;
  int minimum_background_threads = 1;
  int maximum_background_threads = 1;
};

struct ClientConfig {
  std::string client;
  std::string transport;
  std::string path;
};

bool operator<(ClientConfig const& lhs, ClientConfig const& rhs) {
  return std::tie(lhs.client, lhs.transport, lhs.path) <
         std::tie(rhs.client, rhs.transport, rhs.path);
}

struct IterationConfig {
  int iteration;
  ClientConfig cc;
  std::uint64_t transfer_size;
  int background_threads;
  int concurrency;
};

struct Result {
  IterationConfig iteration_config;
  std::string operation;
  int repeat;
  std::chrono::system_clock::time_point batch_start;
  std::chrono::system_clock::time_point transfer_start;
  std::chrono::nanoseconds elapsed;
  // These are useful when debugging problems
  std::string object_name;
  std::int64_t generation;
  std::string peer;
  std::string transfer_id;
  g::Status status;
};

std::string FormatStatus(g::Status const& s) {
  if (s.ok()) return "OK";
  std::ostringstream os;
  os << s;
  auto formatted = std::move(os).str();
  // Make the output (mostly) safe on CSV files
  std::replace(formatted.begin(), formatted.end(), ',', ';');
  std::replace(formatted.begin(), formatted.end(), '\n', ';');
  return formatted;
}

std::string Header() {
  return "Iteration,Operation,Repeat"
         ",Client,Transport,Path"
         ",TransferSize,BackgroundThreads,Concurrency"
         ",BatchStart,TransferStart,Elapsed"
         ",Bucket,ObjectName,Generation,Peer,TransferId,Status,Labels";
}

std::ostream& FormatResult(std::ostream& os, Configuration const& cfg,
                           Result const& r) {
  using ::google::cloud::storage_benchmarks::FormatTimestamp;
  auto const& i = r.iteration_config;
  return os << i.iteration                               //
            << ',' << r.operation                        //
            << ',' << r.repeat                           //
            << ',' << i.cc.client                        //
            << ',' << i.cc.transport                     //
            << ',' << i.cc.path                          //
            << ',' << i.transfer_size                    //
            << ',' << i.background_threads               //
            << ',' << i.concurrency                      //
            << ',' << FormatTimestamp(r.batch_start)     //
            << ',' << FormatTimestamp(r.transfer_start)  //
            << ',' << r.elapsed.count()                  //
            << ',' << cfg.bucket                         //
            << ',' << r.object_name                      //
            << ',' << r.generation                       //
            << ',' << r.peer                             //
            << ',' << r.transfer_id                      //
            << ',' << FormatStatus(r.status)             //
            << ',' << cfg.labels                         //
      ;
}

auto MakeClientConfigs(Configuration const& cfg) {
  auto const valid = std::set<ClientConfig>{
      {kAsyncClientName, kGrpc, "CP"}, {kAsyncClientName, kGrpc, "DP"},
      {kSyncClientName, kGrpc, "CP"},  {kSyncClientName, kGrpc, "DP"},
      {kSyncClientName, kJson, "CP"},
  };
  std::set<ClientConfig> cross;
  for (auto const& c : cfg.clients) {
    for (auto const& t : cfg.transports) {
      for (auto const& p : cfg.paths) cross.emplace(c, t, p);
    }
  }
  std::set<ClientConfig> r;
  std::set_intersection(cross.begin(), cross.end(), valid.begin(), valid.end(),
                        std::inserter(r, r.end()));
  return r;
}

auto MapPath(std::string_view path) {
  if (path == "CP") return std::string("storage.googleapis.com");
  return std::string("google-c2p:///storage.googleapis.com");
}

auto MakeAsyncClients(Configuration const& cfg,
                      std::set<ClientConfig> const& clients,
                      int background_threads) {
  std::map<ClientConfig, gcs_ex::AsyncClient> result;
  for (auto const& cc : clients) {
    if (cc.client != kAsyncClientName) continue;
    result.emplace(
        cc, gcs_ex::AsyncClient(g::Options()
                                    .set<g::GrpcBackgroundThreadPoolSizeOption>(
                                        background_threads)
                                    .set<g::EndpointOption>(MapPath(cc.path))));
  }
  return result;
}

auto MakeClient(ClientConfig const& cc, int background_threads) {
  if (cc.transport == "GRPC") {
    return google::cloud::storage_experimental::DefaultGrpcClient(
        g::Options{}
            .set<g::GrpcBackgroundThreadPoolSizeOption>(background_threads)
            .set<g::EndpointOption>(MapPath(cc.path)));
  }
  return gcs::Client(g::Options{}.set<g::EndpointOption>(MapPath(cc.path)));
}

auto MakeSyncClients(Configuration const& cfg,
                     std::set<ClientConfig> const& clients,
                     int background_threads) {
  std::map<ClientConfig, gcs::Client> result;
  for (auto const& cc : clients) {
    if (cc.client != kSyncClientName) continue;
    result.emplace(cc, MakeClient(cc, background_threads));
  }
  return result;
}

using IterationResult = std::vector<Result>;

auto RandomObjectSize(g::internal::DefaultPRNG& gen, Configuration const& cfg) {
  return std::uniform_int_distribution<std::uint64_t>(
      cfg.minimum_object_size, cfg.maximum_object_size)(gen);
}

auto RandomObjectCount(g::internal::DefaultPRNG& gen,
                       Configuration const& cfg) {
  return std::uniform_int_distribution<std::size_t>(
      cfg.minimum_object_count, cfg.maximum_object_count)(gen);
}

auto RandomObjectNames(g::internal::DefaultPRNG& gen,
                       Configuration const& cfg) {
  auto count = RandomObjectCount(gen, cfg);
  using ::google::cloud::storage_benchmarks::MakeRandomObjectName;
  std::vector<std::string> object_names;
  std::generate_n(std::back_inserter(object_names), count,
                  [&gen] { return MakeRandomObjectName(gen); });
  return object_names;
}

std::string GetPeer(ClientConfig const& cc,
                    std::multimap<std::string, std::string> const& headers) {
  auto l =
      headers.find(cc.transport == kJson ? ":curl-peer" : ":grpc-context-peer");
  if (l == headers.end()) return kMissingUploadId;
  return l->second;
}

std::string GetPeer(ClientConfig const& cc,
                    google::cloud::RpcMetadata const& metadata) {
  return GetPeer(cc, metadata.headers);
}

std::string GetTransferId(
    ClientConfig const& cc,
    std::multimap<std::string, std::string> const& headers) {
  auto l = headers.find("x-guploader-uploadid");
  if (l == headers.end()) return kMissingUploadId;
  return l->second;
}

std::string GetTransferId(ClientConfig const& cc,
                          google::cloud::RpcMetadata const& metadata) {
  return GetTransferId(cc, metadata.headers);
}

IterationResult AppendSummary(std::chrono::steady_clock::time_point start,
                              IterationResult batch) {
  // If there is nothing to "batch" return the plain results.
  if (batch.size() <= 1) return batch;

  auto const elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now() - start);

  auto const& p = batch.front();
  auto iteration = p.iteration_config;
  iteration.transfer_size = std::accumulate(
      batch.begin(), batch.end(), std::uint64_t{0}, [](auto a, auto const& b) {
        return a + b.iteration_config.transfer_size;
      });
  auto l = std::find_if(batch.begin(), batch.end(),
                        [](auto const& r) { return !r.status.ok(); });
  auto status = l == batch.end() ? g::Status{} : l->status;

  batch.emplace_back(std::move(iteration), p.operation + "/BATCH", /*repeat=*/0,
                     p.batch_start, p.batch_start, elapsed,
                     /*object_name=*/"",
                     /*generation=*/0,
                     /*peer=*/kMissingPeer,
                     /*transfer_id=*/kMissingUploadId,
                     /*status=*/std::move(status));
  return batch;
}

g::future<IterationResult> WaitBatch(
    std::chrono::steady_clock::time_point start,
    std::vector<g::future<Result>> pending) {
  IterationResult batch;
  for (auto& r : pending) batch.push_back(co_await std::move(r));
  co_return AppendSummary(start, std::move(batch));
}

Result MakeResult(Configuration const& cfg, IterationConfig iteration,
                  std::string operation, int repeat,
                  std::chrono::system_clock::time_point batch_start,
                  std::string object_name, std::int64_t generation,
                  google::cloud::RpcMetadata const& metadata,
                  std::chrono::system_clock::time_point transfer_start,
                  std::chrono::nanoseconds elapsed) {
  auto peer = GetPeer(iteration.cc, metadata);
  auto transfer_id = GetTransferId(iteration.cc, metadata);
  return Result{std::move(iteration),
                std::move(operation),
                repeat,
                batch_start,
                transfer_start,
                elapsed,
                std::move(object_name),
                generation,
                std::move(peer),
                std::move(transfer_id),
                g::Status{}};
}

Result MakeResult(Configuration const& cfg, IterationConfig iteration,
                  std::string operation, int repeat,
                  std::chrono::system_clock::time_point batch_start,
                  std::string object_name, std::int64_t generation,
                  gcs::HeadersMap const& headers,
                  std::chrono::system_clock::time_point transfer_start,
                  std::chrono::nanoseconds elapsed) {
  auto peer = GetPeer(iteration.cc, headers);
  auto transfer_id = GetTransferId(iteration.cc, headers);
  return Result{std::move(iteration),
                std::move(operation),
                repeat,
                batch_start,
                transfer_start,
                elapsed,
                std::move(object_name),
                generation,
                std::move(peer),
                std::move(transfer_id),
                g::Status{}};
}

Result MakeErrorResult(Configuration const& cfg, IterationConfig iteration,
                       std::string operation, int repeat,
                       std::chrono::system_clock::time_point batch_start,
                       std::string object_name, g::Status status) {
  auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::system_clock::now() - batch_start);
  return Result{std::move(iteration),
                std::move(operation),
                repeat,
                batch_start,
                batch_start,
                elapsed,
                object_name,
                /*.generation=*/0,
                /*.peer=*/kMissingPeer,
                /*.transfer_id=*/kMissingUploadId,
                std::move(status)};
}

g::future<Result> DownloadOne(Configuration const& cfg,
                              IterationConfig iteration, int repeat,
                              gcs_ex::AsyncClient client,
                              std::chrono::system_clock::time_point batch_start,
                              std::string object_name) try {
  auto const transfer_start = std::chrono::system_clock::now();
  auto const start = std::chrono::steady_clock::now();
  auto [reader, token] =
      (co_await client.ReadObject(gcs_ex::BucketName(cfg.bucket), object_name))
          .value();

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

  co_return MakeResult(cfg, std::move(iteration), "READ", repeat, batch_start,
                       std::move(object_name), generation,
                       reader.GetRequestMetadata(), transfer_start,
                       std::chrono::steady_clock::now() - start);
} catch (g::RuntimeStatusError const& ex) {
  co_return MakeErrorResult(cfg, std::move(iteration), "READ", repeat,
                            batch_start, std::move(object_name), ex.status());
}

g::future<IterationResult> Download(Configuration const& cfg,
                                    IterationConfig iteration, int repeat,
                                    gcs_ex::AsyncClient client,
                                    std::vector<std::string> object_names) {
  std::vector<g::future<Result>> batch;
  auto const batch_start = std::chrono::system_clock::now();
  auto const start = std::chrono::steady_clock::now();
  for (auto& name : object_names) {
    batch.push_back(DownloadOne(cfg, iteration, repeat, client, batch_start,
                                std::move(name)));
  }
  return WaitBatch(start, std::move(batch));
}

g::future<Result> UploadOne(Configuration const& cfg, IterationConfig iteration,
                            int repeat, gcs_ex::AsyncClient client,
                            std::chrono::system_clock::time_point batch_start,
                            std::shared_ptr<std::string const> data,
                            std::string object_name) try {
  auto const transfer_start = std::chrono::system_clock::now();
  auto const start = std::chrono::steady_clock::now();
  auto [writer, token] = (co_await client.StartUnbufferedUpload(
                              gcs_ex::BucketName(cfg.bucket), object_name))
                             .value();

  using google::cloud::storage_experimental::WritePayload;
  for (auto remaining = iteration.transfer_size; remaining != 0;) {
    if (!token.valid()) break;
    auto const n =
        std::min(static_cast<std::uint64_t>(data->size()), remaining);
    remaining -= n;
    // This copy is intentional. The benchmark is more realistic if we assume
    // the source data has to be copied into the payload.
    auto payload = WritePayload(data->substr(0, static_cast<std::size_t>(n)));
    token =
        (co_await writer.Write(std::move(token), std::move(payload))).value();
  }

  auto metadata = (co_await writer.Finalize(std::move(token))).value();
  co_return MakeResult(cfg, std::move(iteration), "WRITE", repeat, batch_start,
                       std::move(object_name), metadata.generation(),
                       writer.GetRequestMetadata(), transfer_start,
                       std::chrono::steady_clock::now() - start);
} catch (g::RuntimeStatusError const& ex) {
  co_return MakeErrorResult(cfg, std::move(iteration), "WRITE", repeat,
                            batch_start, std::move(object_name), ex.status());
}

g::future<IterationResult> Upload(Configuration const& cfg,
                                  IterationConfig iteration, int repeat,
                                  gcs_ex::AsyncClient client,
                                  std::shared_ptr<std::string const> data,
                                  std::vector<std::string> object_names) {
  std::vector<g::future<Result>> batch;
  auto const batch_start = std::chrono::system_clock::now();
  auto const start = std::chrono::steady_clock::now();
  for (auto& name : object_names) {
    batch.push_back(UploadOne(cfg, iteration, repeat, client, batch_start, data,
                              std::move(name)));
  }
  return WaitBatch(start, std::move(batch));
}

template <typename F, typename... Args>
auto Launch(F&& function, Args&&... a)
    -> g::future<std::invoke_result_t<F, Args...>> {
  using R = std::invoke_result_t<F, Args...>;
  g::promise<R> p;
  auto f = p.get_future();
  // Detached threads are not a great practice, but in a benchmark, where at
  // least the caller waits until `p.set_value()` it is not that bad.
  std::thread(
      [p = std::move(p)](auto&& function, auto&&... args) mutable {
        p.set_value(function(std::forward<decltype(args)>(args)...));
      },
      std::forward<F>(function), std::forward<Args>(a)...)
      .detach();
  return f;
}

IterationResult WaitTasks(std::chrono::steady_clock::time_point start,
                          std::vector<g::future<IterationResult>> tasks) {
  std::vector<Result> batch;
  for (auto& t : tasks) {
    auto r = t.get();
    batch.insert(batch.end(), std::make_move_iterator(r.begin()),
                 std::make_move_iterator(r.end()));
  }
  return AppendSummary(start, std::move(batch));
}

Result DownloadOne(Configuration const& cfg, IterationConfig iteration,
                   int repeat, gcs::Client client,
                   std::chrono::system_clock::time_point batch_start,
                   std::string object_name) try {
  auto const transfer_start = std::chrono::system_clock::now();
  auto const start = std::chrono::steady_clock::now();
  auto reader = client.ReadObject(cfg.bucket, object_name);

  std::uint64_t size = 0;
  std::vector<char> buffer(1024 * 1024L);
  while (reader) {
    reader.read(buffer.data(), buffer.size());
    size += reader.gcount();
  }
  auto const generation = reader.generation().value_or(0);

  return MakeResult(cfg, std::move(iteration), "READ", repeat, batch_start,
                    std::move(object_name), generation, reader.headers(),
                    transfer_start, std::chrono::steady_clock::now() - start);
} catch (g::RuntimeStatusError const& ex) {
  return MakeErrorResult(cfg, std::move(iteration), "READ", repeat, batch_start,
                         std::move(object_name), ex.status());
}

g::future<IterationResult> Download(Configuration const& cfg,
                                    IterationConfig iteration, int repeat,
                                    gcs::Client client,
                                    std::vector<std::string> object_names) {
  auto download = [=](std::chrono::system_clock::time_point batch_start,
                      int task_id) {
    std::vector<Result> batch;
    int count = 0;
    for (auto& name : object_names) {
      if (count++ % iteration.concurrency != task_id) continue;
      batch.push_back(DownloadOne(cfg, iteration, repeat, client, batch_start,
                                  std::move(name)));
    }
    return batch;
  };

  std::vector<g::future<IterationResult>> tasks;
  auto const batch_start = std::chrono::system_clock::now();
  auto const start = std::chrono::steady_clock::now();
  for (int i = 0; i != iteration.concurrency; ++i) {
    tasks.push_back(Launch(download, batch_start, i));
  }
  return Launch(WaitTasks, start, std::move(tasks));
}

Result UploadOne(Configuration const& cfg, IterationConfig iteration,
                 int repeat, gcs::Client client,
                 std::chrono::system_clock::time_point batch_start,
                 std::shared_ptr<std::string const> data,
                 std::string object_name) try {
  auto const transfer_start = std::chrono::system_clock::now();
  auto const start = std::chrono::steady_clock::now();
  auto writer = client.WriteObject(cfg.bucket, object_name);
  for (auto remaining = iteration.transfer_size; remaining != 0 && writer;) {
    auto const n =
        std::min(static_cast<std::uint64_t>(data->size()), remaining);
    writer.write(data->data(), n);
    remaining -= n;
  }
  writer.Close();

  auto metadata = writer.metadata().value();
  return MakeResult(cfg, std::move(iteration), "WRITE", repeat, batch_start,
                    std::move(object_name), metadata.generation(),
                    writer.headers(), transfer_start,
                    std::chrono::steady_clock::now() - start);
} catch (g::RuntimeStatusError const& ex) {
  return MakeErrorResult(cfg, std::move(iteration), "WRITE", repeat,
                         batch_start, std::move(object_name), ex.status());
}

g::future<IterationResult> Upload(Configuration const& cfg,
                                  IterationConfig iteration, int repeat,
                                  gcs::Client client,
                                  std::shared_ptr<std::string const> data,
                                  std::vector<std::string> object_names) {
  auto upload = [=](std::chrono::system_clock::time_point batch_start,
                    int task_id) {
    std::vector<Result> batch;
    int count = 0;
    for (auto& name : object_names) {
      if (count++ % iteration.concurrency != task_id) continue;
      batch.push_back(UploadOne(cfg, iteration, repeat, client, batch_start,
                                data, std::move(name)));
    }
    return batch;
  };

  std::vector<g::future<IterationResult>> tasks;
  auto const batch_start = std::chrono::system_clock::now();
  auto const start = std::chrono::steady_clock::now();
  for (int i = 0; i != iteration.concurrency; ++i) {
    tasks.push_back(Launch(upload, batch_start, i));
  }

  return Launch(WaitTasks, start, std::move(tasks));
}

void PrintResults(Configuration const& cfg,
                  std::vector<g::future<IterationResult>> results) {
  for (auto& f : results) {
    for (auto const& r : f.get()) FormatResult(std::cout, cfg, r) << "\n";
  }
}

void RunBenchmark(Configuration const& cfg) {
  using ::google::cloud::storage_benchmarks::FormatTimestamp;
  using ::google::cloud::storage_benchmarks::MakeRandomData;
  using ::google::cloud::testing_util::FormatSize;

  std::cout << "# " << FormatTimestamp(std::chrono::system_clock::now())
            << "\n# Labels: " << cfg.labels                               //
            << "\n# Bucket: " << cfg.bucket                               //
            << "\n# Iterations: " << cfg.iterations                       //
            << "\n# Minimum Concurrency: " << cfg.minimum_concurrency     //
            << "\n# Maximum Concurrency: " << cfg.maximum_concurrency     //
            << "\n# Minimum Background Threads: "                         //
            << cfg.minimum_background_threads                             //
            << "\n# Maximum Background Threads: "                         //
            << cfg.maximum_background_threads                             //
            << "\n# Minimum Object Count: "                               //
            << cfg.minimum_object_count                                   //
            << "\n# Maximum Object Count: "                               //
            << cfg.maximum_object_count                                   //
            << "\n# Minimum Object Size: "                                //
            << FormatSize(cfg.minimum_object_size)                        //
            << "\n# Maximum Object Size: "                                //
            << FormatSize(cfg.maximum_object_size)                        //
            << "\n# Clients: " << absl::StrJoin(cfg.clients, ", ")        //
            << "\n# Transports: " << absl::StrJoin(cfg.transports, ", ")  //
            << "\n# Paths: " << absl::StrJoin(cfg.paths, ", ")            //
            << "\n# Minimum Write Count: " << cfg.minimum_write_count     //
            << "\n# Maximum Write Count: " << cfg.maximum_write_count     //
            << "\n# Minimum Read Count: " << cfg.minimum_read_count       //
            << "\n# Maximum Read Count: " << cfg.maximum_read_count       //
            << "\n# Compiler: " << g::internal::compiler()                //
            << "\n# Flags: " << g::internal::compiler_flags()             //
            << std::endl;

  auto gen = google::cloud::internal::MakeDefaultPRNG();
  auto const data =
      std::make_shared<std::string const>(MakeRandomData(gen, cfg.chunk_size));

  auto const client_configs = MakeClientConfigs(cfg);

  std::cout << Header() << "\n";
  auto last_upload = std::chrono::steady_clock::now();

  auto delete_client = gcs_ex::AsyncClient();
  std::vector<g::future<void>> pending_deletes;
  auto delete_all = [](gcs_ex::AsyncClient client, std::string bucket,
                       std::vector<std::string> names) -> g::future<void> {
    std::vector<g::future<g::Status>> pending(names.size());
    std::transform(
        names.begin(), names.end(), pending.begin(), [&](auto const& name) {
          return client.DeleteObject(gcs_ex::BucketName(bucket), name);
        });
    names.clear();
    for (auto& p : pending) co_await std::move(p);
  };

  for (int i = 0; i != cfg.iterations; ++i) {
    auto const object_size = RandomObjectSize(gen, cfg);
    auto const names = RandomObjectNames(gen, cfg);
    auto const write_count = std::uniform_int_distribution<int>(
        cfg.minimum_write_count, cfg.maximum_write_count)(gen);
    auto const read_count = std::uniform_int_distribution<int>(
        cfg.minimum_read_count, cfg.maximum_read_count)(gen);
    auto const background_threads = std::uniform_int_distribution<int>(
        cfg.minimum_background_threads, cfg.maximum_background_threads)(gen);
    auto const concurrency = std::uniform_int_distribution<int>(
        cfg.minimum_concurrency, cfg.maximum_concurrency)(gen);
    auto const sync_clients =
        MakeSyncClients(cfg, client_configs, background_threads);
    auto const async_clients =
        MakeAsyncClients(cfg, client_configs, background_threads);

    for (int w = 0; w != write_count; ++w) {
      std::vector<g::future<IterationResult>> uploads;
      for (auto const& [cc, client] : async_clients) {
        uploads.push_back(
            Upload(cfg,
                   IterationConfig{i, cc, object_size, background_threads,
                                   concurrency},
                   w, client, data, names));
      }
      for (auto const& [cc, client] : sync_clients) {
        uploads.push_back(
            Upload(cfg,
                   IterationConfig{i, cc, object_size, background_threads,
                                   concurrency},
                   w, client, data, names));
      }
      PrintResults(cfg, std::move(uploads));
    }

    for (int r = 0; r != read_count; ++r) {
      std::vector<g::future<IterationResult>> downloads;
      for (auto const& [cc, client] : async_clients) {
        downloads.push_back(
            Download(cfg,
                     IterationConfig{i, cc, object_size, background_threads,
                                     concurrency},
                     r, client, names));
      }
      for (auto const& [cc, client] : sync_clients) {
        downloads.push_back(
            Download(cfg,
                     IterationConfig{i, cc, object_size, background_threads,
                                     concurrency},
                     r, client, names));
      }
      PrintResults(cfg, std::move(downloads));
    }

    pending_deletes.push_back(
        delete_all(delete_client, cfg.bucket, std::move(names)));
  }
  for (auto& p : pending_deletes) p.get();
  std::cout << "# DONE\n";
}

Configuration ParseArgs(std::vector<std::string> argv) {
  using ::google::cloud::storage_benchmarks::ParseBoolean;
  using ::google::cloud::storage_benchmarks::ParseSize;
  using ::google::cloud::testing_util::OptionDescriptor;

  bool help = false;
  bool description = false;
  Configuration cfg;
  std::vector<OptionDescriptor> desc{
      {"--help", "print usage information",
       [&help](std::string const&) { help = true; }},
      {"--description", "print benchmark description",
       [&description](std::string const&) { description = true; }},
      {"--labels", "label the benchmark results",
       [&cfg](std::string const& v) {
         cfg.labels = v;
         std::replace(cfg.labels.begin(), cfg.labels.end(), ',', ';');
       }},
      {"--bucket", "select the bucket for the benchmark",
       [&cfg](std::string const& v) { cfg.bucket = v; }},
      {"--iterations", "select the number of iterations in the benchmark",
       [&cfg](std::string const& v) { cfg.iterations = std::stoi(v); }},

      {"--minimum-object-count", "select the minimum object count",
       [&cfg](std::string const& v) {
         cfg.minimum_object_count = std::stoi(v);
       }},
      {"--maximum-object-count", "select the maximum object count",
       [&cfg](std::string const& v) {
         cfg.maximum_object_count = std::stoi(v);
       }},
      {"--minimum-object-size", "select the minimum object size",
       [&cfg](std::string const& v) {
         cfg.minimum_object_size = ParseSize(v);
       }},
      {"--maximum-object-size", "select the maximum object size",
       [&cfg](std::string const& v) {
         cfg.maximum_object_size = ParseSize(v);
       }},
      {"--chunk-size", "select the upload chunk size",
       [&cfg](std::string const& v) { cfg.chunk_size = ParseSize(v); }},
      {"--clients", "select the clients",
       [&cfg](std::string const& v) { cfg.clients = absl::StrSplit(v, ','); }},
      {"--transports", "select the transports",
       [&cfg](std::string const& v) {
         cfg.transports = absl::StrSplit(v, ',');
       }},
      {"--minimum-read-count", "select the minimum read count",
       [&cfg](std::string const& v) { cfg.minimum_read_count = std::stoi(v); }},
      {"--maximum-read-count", "select the maximum read count",
       [&cfg](std::string const& v) { cfg.maximum_read_count = std::stoi(v); }},
      {"--minimum-write-count", "select the minimum write count",
       [&cfg](std::string const& v) {
         cfg.minimum_write_count = std::stoi(v);
       }},
      {"--maximum-write-count", "select the maximum write count",
       [&cfg](std::string const& v) {
         cfg.maximum_write_count = std::stoi(v);
       }},
      {"--paths", "select the communication paths",
       [&cfg](std::string const& v) { cfg.paths = absl::StrSplit(v, ','); }},
      {"--minimum-concurrency", "number of concurrent transfers",
       [&cfg](std::string const& v) {
         cfg.minimum_concurrency = std::stoi(v);
       }},
      {"--maximum-concurrency", "number of concurrent transfers",
       [&cfg](std::string const& v) {
         cfg.maximum_concurrency = std::stoi(v);
       }},
      {"--minimum-background-threads",
       "configure the number of background threads",
       [&cfg](std::string const& v) {
         cfg.minimum_background_threads = std::stoi(v);
       }},
      {"--maximum-background-threads",
       "configure the number of background threads",
       [&cfg](std::string const& v) {
         cfg.maximum_background_threads = std::stoi(v);
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
  if (cfg.bucket.empty()) {
    throw std::invalid_argument("empty value for --bucket option");
  }
  return cfg;
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
