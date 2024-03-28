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

#include "google/cloud/storage/benchmarks/benchmark_utils.h"
#include "google/cloud/storage/benchmarks/throughput_experiment.h"
#include "google/cloud/storage/benchmarks/throughput_options.h"
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
#include <functional>
#include <future>
#include <set>
#include <sstream>

namespace {
namespace gcs = ::google::cloud::storage;
namespace gcs_bm = ::google::cloud::storage_benchmarks;
using gcs_bm::ExperimentLibrary;
using gcs_bm::ExperimentTransport;
using gcs_bm::ThroughputOptions;

char const kDescription[] = R"""(
A throughput vs. CPU benchmark for the Google Cloud Storage C++ client library.

This program measures the throughput and CPU utilization when uploading
and downloading objects using the Google Cloud Storage (GCS) C++ client library.
The program repeats the "experiment" of uploading, then downloading, and then
removing an object many times, using a randomly selected size in each iteration.
An external script presents these results as a series of plots.

The program first creates a GCS bucket that will contain all the objects used
by that run of the program. The name of this bucket is selected at random, so
multiple copies of the program can run simultaneously. The bucket is deleted at
the end of the run of this program. The bucket uses the `STANDARD` storage
class, in a region set via the command line. Choosing regions close to where the
program is running can be used to estimate the latency without any wide-area
network effects. Choosing regions far from where the program is running can be
used to evaluate the performance of the library when the wide-area network is
taken into account.

After creating this bucket the program creates a number of threads, configurable
via the command line, to obtain more samples in parallel. Configure this value
with a small enough number of threads such that you do not saturate the CPU.

Each thread creates C++ objects to perform the "upload experiments". Each one
of these objects represents the "api" used to perform the upload, that is JSON
and/or gRPC (though technically gRPC is just another protocol for the JSON
API). Likewise, the thread creates a number of "download experiments", also
based on the APIs configured via the command-line.

Then the thread repeats the following steps (see below for the conditions to
stop the loop):

- Select a random size, between two values configured in the command line of the
  object to upload.
- The application buffer sizes for `read()` and `write()` calls are also
  selected at random. These sizes are quantized, and the quantum can be
  configured in the command-line.
- Select a random uploader, that is, which API will be used to upload the
  object.
- Select a random downloader, that is, which API will be used to download the
  object.
- Select, at random, if the client library will perform CRC32C and/or MD5 hashes
  on the uploaded and downloaded data.
- Upload an object of the selected size, choosing the name of the object at
  random.
- Once the object is fully uploaded, the program captures the object size, the
  write buffer size, the elapsed time (in microseconds), the CPU time
  (in microseconds) used during the upload, and the status code for the upload.
- Then the program downloads the same object (3 times), and captures the object
  size, the read buffer size, the elapsed time (in microseconds), the CPU time
  (in microseconds) used during the download, and the status code for the
  download.
- The program then deletes this object and starts another iteration.

The loop stops when any of the following conditions are met:

- The test has obtained more than a prescribed "maximum number of samples"
- The test has obtained at least a prescribed "minimum number of samples" *and*
  the test has been running for more than a prescribed "duration".

Once the threads finish running their loops the program prints the captured
performance data. The bucket is deleted after the program terminates.

A helper script in this directory can generate pretty graphs from the output of
this program.
)""";

using ResultHandler =
    std::function<void(ThroughputOptions const&, gcs_bm::ThroughputResult)>;

gcs_bm::ClientProvider MakeProvider(ThroughputOptions const& options);

void RunThread(ThroughputOptions const& ThroughputOptions, int thread_id,
               ResultHandler const& handler,
               gcs_bm::ClientProvider const& provider);

google::cloud::StatusOr<ThroughputOptions> ParseArgs(int argc, char* argv[]);

}  // namespace

int main(int argc, char* argv[]) {
  google::cloud::StatusOr<ThroughputOptions> options = ParseArgs(argc, argv);
  if (!options) {
    std::cerr << options.status() << "\n";
    return 1;
  }
  if (options->exit_after_parse) return 0;

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

  auto output_size_range = [](std::string const& name, auto minimum,
                              auto maximum) {
    std::cout << "\n# " << name << " Range: [" << minimum << ',' << maximum
              << ']';
  };

  auto output_quantized_range = [](std::string const& name, auto minimum,
                                   auto maximum, auto quantum) {
    std::cout << "\n# " << name << " Range: [" << minimum << ',' << maximum
              << "]\n# " << name << " Quantum: " << quantum;
  };
  auto output_optional_quantized_range =
      [](std::string const& name, auto minimum, auto maximum, auto quantum) {
        if (!minimum.has_value() || !maximum.has_value()) {
          std::cout << "\n# " << name << " Range: [not set]";
        } else {
          std::cout << "\n# " << name << " Range: [" << *minimum << ','
                    << *maximum << "]";
        }
        std::cout << "\n# " << name << " Quantum: " << quantum;
      };

  std::cout << "# Start time: " << gcs_bm::CurrentTime()          //
            << "\n# Labels: " << options->labels                  //
            << "\n# Running test on bucket: " << options->bucket  //
            << "\n# Duration: "
            << absl::FormatDuration(absl::FromChrono(options->duration))
            << "\n# Thread Count: " << options->thread_count
            << "\n# Client Per Thread: " << options->client_per_thread;

  output_size_range("Object Size", options->minimum_object_size,
                    options->maximum_object_size);
  output_quantized_range(
      "Write Buffer Size", options->minimum_write_buffer_size,
      options->maximum_write_buffer_size, options->write_buffer_quantum);
  output_quantized_range("Read Buffer Size", options->minimum_read_buffer_size,
                         options->maximum_read_buffer_size,
                         options->read_buffer_quantum);

  std::cout << "\n# Minimum Sample Count: " << options->minimum_sample_count
            << "\n# Maximum Sample Count: " << options->maximum_sample_count
            << "\n# Enabled Libs: "
            << absl::StrJoin(options->libs, ",", Formatter{})
            << "\n# Enabled Transports: "
            << absl::StrJoin(options->transports, ",", Formatter{})
            << "\n# Enabled CRC32C: "
            << absl::StrJoin(options->enabled_crc32c, ",", Formatter{})
            << "\n# Enabled MD5: "
            << absl::StrJoin(options->enabled_md5, ",", Formatter{})
            << "\n# Minimum Sample Delay: "
            << absl::FormatDuration(
                   absl::FromChrono(options->minimum_sample_delay));

  gcs_bm::PrintOptions(std::cout, "Common", options->client_options);
  gcs_bm::PrintOptions(std::cout, "Rest", options->rest_options);
  gcs_bm::PrintOptions(std::cout, "Grpc", options->grpc_options);
  gcs_bm::PrintOptions(std::cout, "Direct Path", options->direct_path_options);

  output_optional_quantized_range("Read Offset", options->minimum_read_offset,
                                  options->maximum_read_offset,
                                  options->read_offset_quantum);
  output_optional_quantized_range("Read Size", options->minimum_read_size,
                                  options->maximum_read_size,
                                  options->read_size_quantum);

  std::cout << "\n# Build info: " << notes << "\n";
  // Make the output generated so far immediately visible, helps with debugging.
  std::cout << std::flush;

  // Serialize output to `std::cout`.
  std::mutex mu;
  auto handler = [&mu](ThroughputOptions const& options,
                       gcs_bm::ThroughputResult const& result) {
    std::lock_guard<std::mutex> lk(mu);
    gcs_bm::PrintAsCsv(std::cout, options, result);
    if (!result.status.ok()) {
      google::cloud::LogSink::Instance().Flush();
    }
  };
  auto provider = MakeProvider(*options);

  gcs_bm::PrintThroughputResultHeader(std::cout);
  std::vector<std::future<void>> tasks;
  for (int i = 0; i != options->thread_count; ++i) {
    tasks.emplace_back(std::async(std::launch::async, RunThread, *options, i,
                                  handler, provider));
  }
  for (auto& f : tasks) f.get();

  std::cout << "# DONE\n" << std::flush;

  return 0;
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
      return gcs::MakeGrpcClient(std::move(opts));
    }
    if (t == ExperimentTransport::kGrpc) {
      opts = google::cloud::internal::MergeOptions(options.grpc_options,
                                                   std::move(opts));
      return gcs::MakeGrpcClient(std::move(opts));
    }
#else
    (void)t;  // disable unused parameter warning
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
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

void RunThread(ThroughputOptions const& options, int thread_id,
               ResultHandler const& handler,
               gcs_bm::ClientProvider const& provider) {
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());

  auto uploaders = gcs_bm::CreateUploadExperiments(options, provider);
  if (uploaders.empty()) {
    // This is possible if only gRPC is requested but the benchmark was compiled
    // without gRPC support.
    std::cout << "# None of the APIs configured are available\n";
    return;
  }
  auto downloaders =
      gcs_bm::CreateDownloadExperiments(options, provider, thread_id);
  if (downloaders.empty()) {
    // This is possible if only gRPC is requested but the benchmark was compiled
    // without gRPC support.
    std::cout << "# None of the APIs configured are available\n";
    return;
  }

  std::uniform_int_distribution<std::size_t> uploader_generator(
      0, uploaders.size() - 1);
  std::uniform_int_distribution<std::size_t> downloader_generator(
      0, downloaders.size() - 1);

  std::uniform_int_distribution<std::int64_t> size_generator(
      options.minimum_object_size, options.maximum_object_size);

  auto quantized_range_generator = [](auto minimum, auto maximum,
                                      auto quantum) {
    auto distribution = std::uniform_int_distribution<decltype(quantum)>(
        minimum / quantum, maximum / quantum);
    return [d = std::move(distribution), quantum](auto& g) mutable {
      return quantum * d(g);
    };
  };

  auto write_buffer_size_generator = quantized_range_generator(
      options.minimum_write_buffer_size, options.maximum_write_buffer_size,
      options.write_buffer_quantum);
  auto read_buffer_size_generator = quantized_range_generator(
      options.minimum_read_buffer_size, options.maximum_read_buffer_size,
      options.read_buffer_quantum);

  auto read_range_generator = [&](auto& g, std::int64_t object_size)
      -> absl::optional<std::pair<std::int64_t, std::int64_t>> {
    if (!options.minimum_read_size.has_value() ||
        !options.maximum_read_size.has_value() ||
        !options.minimum_read_offset.has_value() ||
        !options.maximum_read_offset.has_value()) {
      return absl::nullopt;
    }
    auto read_offset_generator = quantized_range_generator(
        *options.minimum_read_offset, *options.maximum_read_offset,
        options.read_offset_quantum);
    auto read_size_generator = quantized_range_generator(
        *options.minimum_read_size, *options.maximum_read_size,
        options.read_size_quantum);
    auto offset = (std::min)(object_size, read_offset_generator(g));
    auto size = (std::min)(object_size - offset, read_size_generator(g));
    // This makes it easier to control the ratio of ranged vs. full reads from
    // the command-line. To make more full reads happen set the read range size
    // to be larger than the object sizes. The larger this read range size is,
    // the higher the proportion of full range reads.
    if (offset == 0 && size == object_size) return absl::nullopt;
    // The REST API has a quirk: reading the last 0 bytes returns all the bytes.
    // Just read the *first* 0 bytes in that case. Note that `size == 0` is
    // implied by the initialization to `min(object_size - offset, ...)`.
    if (offset == object_size) return std::make_pair(0, 0);
    return std::make_pair(offset, size);
  };

  std::uniform_int_distribution<std::size_t> crc32c_generator(
      0, options.enabled_crc32c.size() - 1);
  std::uniform_int_distribution<std::size_t> md5_generator(
      0, options.enabled_crc32c.size() - 1);

  auto deadline = std::chrono::steady_clock::now() + options.duration;

  std::int32_t iteration_count = 0;
  for (auto start = std::chrono::steady_clock::now();
       iteration_count < options.maximum_sample_count &&
       (iteration_count < options.minimum_sample_count || start < deadline);
       start = std::chrono::steady_clock::now(), ++iteration_count) {
    auto const object_name = gcs_bm::MakeRandomObjectName(generator);
    auto const object_size = size_generator(generator);
    auto const write_buffer_size = write_buffer_size_generator(generator);
    auto const read_buffer_size = read_buffer_size_generator(generator);
    bool const enable_crc = options.enabled_crc32c[crc32c_generator(generator)];
    bool const enable_md5 = options.enabled_md5[md5_generator(generator)];
    auto const range = read_range_generator(generator, object_size);

    auto& uploader = uploaders[uploader_generator(generator)];
    auto upload_result = uploader->Run(
        options.bucket, object_name,
        gcs_bm::ThroughputExperimentConfig{
            gcs_bm::kOpWrite, object_size, write_buffer_size, enable_crc,
            enable_md5, /*read_range=*/absl::nullopt});
    auto status = upload_result.status;
    handler(options, std::move(upload_result));

    if (!status.ok()) continue;

    auto& downloader = downloaders[downloader_generator(generator)];
    for (auto op : {gcs_bm::kOpRead0, gcs_bm::kOpRead1, gcs_bm::kOpRead2}) {
      handler(options, downloader->Run(options.bucket, object_name,
                                       gcs_bm::ThroughputExperimentConfig{
                                           op, object_size, read_buffer_size,
                                           enable_crc, enable_md5, range}));
    }
    auto client = provider(ExperimentTransport::kJson);
    (void)client.DeleteObject(options.bucket, object_name);
    // If needed, pace the benchmark so each thread generates only so many
    // samples each second.
    auto const pace = start + options.minimum_sample_delay;
    auto const now = std::chrono::steady_clock::now();
    if (pace > now) std::this_thread::sleep_for(pace - now);
  }
}

google::cloud::StatusOr<ThroughputOptions> SelfTest(char const* argv0) {
  using ::google::cloud::internal::GetEnv;
  using ::google::cloud::internal::Sample;

  auto const bucket_name =
      GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME").value_or("");
  if (bucket_name.empty()) {
    std::ostringstream os;
    os << "The GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME environment variable "
          "is not set or empty";
    return google::cloud::Status(google::cloud::StatusCode::kUnknown,
                                 std::move(os).str());
  }
  return gcs_bm::ParseThroughputOptions(
      {
          argv0,
          "--bucket=" + bucket_name,
          "--thread-count=1",
          "--minimum-object-size=16KiB",
          "--maximum-object-size=32KiB",
          "--minimum-write-buffer-size=16KiB",
          "--maximum-write-buffer-size=128KiB",
          "--write-buffer-quantum=16KiB",
          "--minimum-read-buffer-size=16KiB",
          "--maximum-read-buffer-size=128KiB",
          "--read-buffer-quantum=16KiB",
          "--duration=1s",
          "--minimum-sample-count=4",
          "--maximum-sample-count=10",
          "--enabled-transports=Json",
          "--enabled-crc32c=enabled",
          "--enabled-md5=disabled",
      },
      kDescription);
}

google::cloud::StatusOr<ThroughputOptions> ParseArgs(int argc, char* argv[]) {
  bool auto_run =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES")
          .value_or("") == "yes";
  if (auto_run) return SelfTest(argv[0]);

  auto options =
      gcs_bm::ParseThroughputOptions({argv, argv + argc}, kDescription);
  if (!options) return options;
  // We don't want to get the default labels in the unit tests, as they can
  // flake.
  options->labels = gcs_bm::AddDefaultLabels(std::move(options->labels));
  return options;
}

}  // namespace
