// Copyright 2020 Google LLC
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

#include "google/cloud/storage/benchmarks/throughput_options.h"
#include "absl/strings/str_split.h"
#include "absl/time/time.h"
#include <sstream>

namespace google {
namespace cloud {
namespace storage_benchmarks {
namespace {

Status ValidateQuantizedRange(std::string const& name, std::int64_t minimum,
                              std::int64_t maximum, std::int64_t quantum) {
  using ::google::cloud::StatusCode;
  if (minimum > maximum || minimum < 0 || maximum < 0) {
    std::ostringstream os;
    os << "Invalid range for " << name << " [" << minimum << ',' << maximum
       << "]";
    return google::cloud::Status{StatusCode::kInvalidArgument, os.str()};
  }
  if (quantum <= 0 || (quantum > minimum && minimum != 0)) {
    std::ostringstream os;
    os << "Invalid quantum for " << name << " (" << quantum
       << "), it should be in the (0," << minimum << "] range";
    return google::cloud::Status{StatusCode::kInvalidArgument, os.str()};
  }

  return Status{};
}

std::vector<bool> ParseChecksums(std::string const& val) {
  if (val == "enabled") {
    return {true};
  }
  if (val == "disabled") {
    return {false};
  }
  if (val == "random") {
    return {false, true};
  }
  return {};
}

std::vector<ExperimentLibrary> ParseLibraries(std::string const& val) {
  std::set<ExperimentLibrary> libs;  // avoid duplicates
  for (auto const& token : absl::StrSplit(val, ',')) {
    auto lib = ParseExperimentLibrary(std::string{token});
    if (!lib) return {};
    libs.insert(*lib);
  }
  return {libs.begin(), libs.end()};
}

std::vector<ExperimentTransport> ParseTransports(std::string const& val) {
  std::set<ExperimentTransport> transports;  // avoid duplicates
  for (auto const& token : absl::StrSplit(val, ',')) {
    auto transport = ParseExperimentTransport(std::string{token});
    if (!transport) return {};
    transports.insert(*transport);
  }
  return {transports.begin(), transports.end()};
}

}  // namespace

using ::google::cloud::testing_util::OptionDescriptor;

google::cloud::StatusOr<ThroughputOptions> ParseThroughputOptions(
    std::vector<std::string> const& argv, std::string const& description) {
  ThroughputOptions options;
  bool wants_help = false;
  bool wants_description = false;

  std::vector<OptionDescriptor> desc{
      {"--help", "print usage information",
       [&wants_help](std::string const&) { wants_help = true; }},
      {"--description", "print benchmark description",
       [&wants_description](std::string const&) { wants_description = true; }},
      {"--project-id", "use the given project id for the benchmark",
       [&options](std::string const& val) { options.project_id = val; }},
      {"--region", "use the given region for the benchmark",
       [&options](std::string const& val) { options.region = val; }},
      {"--bucket-prefix", "use this prefix when creating the bucket",
       [&options](std::string const& val) { options.bucket_prefix = val; }},
      {"--thread-count", "set the number of threads in the benchmark",
       [&options](std::string const& val) {
         options.thread_count = std::stoi(val);
       }},
      {"--client-per-thread", "use a separate client on each thread",
       [&options](std::string const& val) {
         options.client_per_thread = ParseBoolean(val).value_or(false);
       }},
      {"--grpc-channel-count",
       "number of gRPC channels created by the client library, use 0 for the "
       "default",
       [&options](std::string const& val) {
         options.grpc_channel_count = std::stoi(val);
       }},
      {"--direct-path-channel-count",
       "number of DirectPath gRPC channels created by the client library, use "
       "0 for the default",
       [&options](std::string const& val) {
         options.direct_path_channel_count = std::stoi(val);
       }},
      {"--minimum-object-size", "configure the minimum object size",
       [&options](std::string const& val) {
         options.minimum_object_size = ParseSize(val);
       }},
      {"--maximum-object-size", "configure the maximum object size",
       [&options](std::string const& val) {
         options.maximum_object_size = ParseSize(val);
       }},
      {"--minimum-write-buffer-size",
       "configure the minimum buffer size for write() calls",
       [&options](std::string const& val) {
         options.minimum_write_buffer_size = ParseBufferSize(val);
       }},
      {"--maximum-write-buffer-size",
       "configure the maximum buffer size for write() calls",
       [&options](std::string const& val) {
         options.maximum_write_buffer_size = ParseBufferSize(val);
       }},
      {"--write-buffer-quantum", "quantize the buffer sizes for write() calls",
       [&options](std::string const& val) {
         options.write_buffer_quantum = ParseBufferSize(val);
       }},
      {"--minimum-read-buffer-size",
       "configure the minimum buffer size for read() calls",
       [&options](std::string const& val) {
         options.minimum_read_buffer_size = ParseBufferSize(val);
       }},
      {"--maximum-read-buffer-size",
       "configure the maximum buffer size for read() calls",
       [&options](std::string const& val) {
         options.maximum_read_buffer_size = ParseBufferSize(val);
       }},
      {"--read-buffer-quantum", "quantize the buffer sizes for read() calls",
       [&options](std::string const& val) {
         options.read_buffer_quantum = ParseBufferSize(val);
       }},
      {"--duration", "continue the test for at least this amount of time",
       [&options](std::string const& val) {
         options.duration = ParseDuration(val);
       }},
      {"--minimum-sample-count",
       "continue the test until at least this number of samples are obtained",
       [&options](std::string const& val) {
         options.minimum_sample_count = std::stoi(val);
       }},
      {"--maximum-sample-count",
       "stop the test when this number of samples are obtained",
       [&options](std::string const& val) {
         options.maximum_sample_count = std::stoi(val);
       }},
      {"--enabled-libs", "enable more libraries (e.g. Raw, CppClient)",
       [&options](std::string const& val) {
         options.libs = ParseLibraries(val);
       }},
      {"--enabled-transports",
       "enable a subset of the transports (DirectPath, Grpc, Json, Xml)",
       [&options](std::string const& val) {
         options.transports = ParseTransports(val);
       }},
      {"--enabled-crc32c", "run with CRC32C enabled, disabled, or both",
       [&options](std::string const& val) {
         options.enabled_crc32c = ParseChecksums(val);
       }},
      {"--enabled-md5", "run with MD5 enabled, disabled, or both",
       [&options](std::string const& val) {
         options.enabled_md5 = ParseChecksums(val);
       }},
      {"--rest-endpoint", "sets the endpoint for REST-based benchmarks",
       [&options](std::string const& val) { options.rest_endpoint = val; }},
      {"--grpc-endpoint", "sets the endpoint for gRPC-based benchmarks",
       [&options](std::string const& val) { options.grpc_endpoint = val; }},
      {"--direct-path-endpoint",
       "sets the endpoint for gRPC+DirectPath-based benchmarks",
       [&options](std::string const& val) {
         options.direct_path_endpoint = val;
       }},
      {"--transfer-stall-timeout",
       "configure the storage::TransferStallTimeoutOption: the maximum time"
       " allowed for data to 'stall' (make no progress) on all operations,"
       " except for downloads (see --download-stall-timeout)."
       " This option is intended for troubleshooting, most of the time the"
       " value is not expected to change the library performance.",
       [&options](std::string const& val) {
         options.transfer_stall_timeout = ParseDuration(val);
       }},
      {"--download-stall-timeout",
       "configure the storage::DownloadStallTimeoutOption: the maximum time"
       " allowed for data to 'stall' during a download."
       " This option is intended for troubleshooting, most of the time the"
       " value is not expected to change the library performance.",
       [&options](std::string const& val) {
         options.download_stall_timeout = ParseDuration(val);
       }},
      {"--minimum-sample-delay",
       "configure the minimum time between samples."
       " Sometimes we only want to collect a few samples per second."
       " This can make the data resulting from a multi-day run more"
       " manageable.",
       [&options](std::string const& val) {
         absl::Duration d;
         if (absl::ParseDuration(val, &d)) {
           options.minimum_sample_delay = absl::ToChronoMilliseconds(d);
         } else {
           options.minimum_sample_delay = std::chrono::milliseconds(-1);
         }
       }},

      {"--minimum-read-offset", "configure the minimum offset of ranged reads",
       [&options](std::string const& val) {
         options.minimum_read_offset = ParseBufferSize(val);
       }},
      {"--maximum-read-offset", "configure the maximum offset for ranged reads",
       [&options](std::string const& val) {
         options.maximum_read_offset = ParseBufferSize(val);
       }},
      {"--read-offset-quantum", "quantize the ranged read offsets",
       [&options](std::string const& val) {
         options.read_offset_quantum = ParseBufferSize(val);
       }},

      {"--minimum-read-size", "configure the minimum size of ranged reads",
       [&options](std::string const& val) {
         options.minimum_read_size = ParseBufferSize(val);
       }},
      {"--maximum-read-size", "configure the maximum size for ranged reads",
       [&options](std::string const& val) {
         options.maximum_read_size = ParseBufferSize(val);
       }},
      {"--read-size-quantum", "quantize the ranged read sizes",
       [&options](std::string const& val) {
         options.read_size_quantum = ParseBufferSize(val);
       }},
      {"--target-api-version-path", "change the API version path for REST",
       [&options](std::string const& val) {
         options.target_api_version_path = val;
       }},
  };
  auto usage = BuildUsage(desc, argv[0]);

  auto unparsed = OptionsParse(desc, argv);
  if (wants_help) {
    std::cout << usage << "\n";
  }

  if (wants_description) {
    std::cout << description << "\n";
  }

  auto make_status = [](std::ostringstream& os) {
    auto const code = google::cloud::StatusCode::kInvalidArgument;
    return google::cloud::Status{code, std::move(os).str()};
  };

  if (unparsed.size() > 2) {
    std::ostringstream os;
    os << "Unknown arguments or options\n" << usage << "\n";
    return make_status(os);
  }
  if (unparsed.size() == 2) {
    options.region = unparsed[1];
  }
  if (options.region.empty()) {
    std::ostringstream os;
    os << "Missing value for --region option\n" << usage << "\n";
    return make_status(os);
  }

  if (options.grpc_channel_count < 0) {
    std::ostringstream os;
    os << "Invalid value for --grpc-channel-count ("
       << options.grpc_channel_count << "), should be >= 0";
    return make_status(os);
  }

  if (options.direct_path_channel_count < 0) {
    std::ostringstream os;
    os << "Invalid value for --direct-path-channel-count ("
       << options.direct_path_channel_count << "), should be >= 0";
    return make_status(os);
  }

  if (options.minimum_object_size > options.maximum_object_size) {
    std::ostringstream os;
    os << "Invalid range for object size [" << options.minimum_object_size
       << ',' << options.maximum_object_size << "]";
    return make_status(os);
  }

  auto status = ValidateQuantizedRange(
      "write buffer size", options.minimum_write_buffer_size,
      options.maximum_write_buffer_size, options.write_buffer_quantum);
  if (!status.ok()) return status;

  status = ValidateQuantizedRange(
      "read buffer size", options.minimum_read_buffer_size,
      options.maximum_read_buffer_size, options.read_buffer_quantum);
  if (!status.ok()) return status;

  if (options.minimum_sample_count > options.maximum_sample_count) {
    std::ostringstream os;
    os << "Invalid range for sample range [" << options.minimum_sample_count
       << ',' << options.maximum_sample_count << "]";
    return make_status(os);
  }

  if (options.thread_count <= 0) {
    std::ostringstream os;
    os << "Invalid --thread-count value (" << options.thread_count
       << "), must be > 0";
    return make_status(os);
  }

  if (!Timer::SupportsPerThreadUsage() && options.thread_count > 1) {
    std::cerr <<
        R"""(
# WARNING
# Your platform does not support per-thread usage metrics and you have enabled
# multiple threads, so the CPU usage results will not be usable. See
# getrusage(2) for more information.
# END WARNING
#
)""";
  }

  if (options.libs.empty()) {
    std::ostringstream os;
    os << "No libraries configured for benchmark. Maybe an invalid name?";
    return make_status(os);
  }

  if (options.transports.empty()) {
    std::ostringstream os;
    os << "No transports configured for benchmark. Maybe an invalid name?";
    return make_status(os);
  }

  if (options.enabled_crc32c.empty()) {
    std::ostringstream os;
    os << "No CRC32C settings configured for benchmark.";
    return make_status(os);
  }

  if (options.enabled_md5.empty()) {
    std::ostringstream os;
    os << "No MD5 settings configured for benchmark.";
    return make_status(os);
  }

  if (options.minimum_sample_delay < std::chrono::milliseconds(0)) {
    std::ostringstream os;
    os << "Invalid value for --minimum-sample-delay";
    return make_status(os);
  }

  status = ValidateQuantizedRange("read offset", options.minimum_read_offset,
                                  options.maximum_read_offset,
                                  options.read_offset_quantum);
  if (!status.ok()) return status;

  status = ValidateQuantizedRange("read size", options.minimum_read_size,
                                  options.maximum_read_size,
                                  options.read_size_quantum);
  if (!status.ok()) return status;

  return options;
}

}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google
