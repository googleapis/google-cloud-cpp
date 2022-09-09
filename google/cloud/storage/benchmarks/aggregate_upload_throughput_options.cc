// Copyright 2021 Google LLC
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

#include "google/cloud/storage/benchmarks/aggregate_upload_throughput_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include <iterator>
#include <sstream>

namespace google {
namespace cloud {
namespace storage_benchmarks {
namespace {

namespace gcs = ::google::cloud::storage;
namespace gcs_ex = ::google::cloud::storage_experimental;
using ::google::cloud::testing_util::OptionDescriptor;

StatusOr<AggregateUploadThroughputOptions> ValidateOptions(
    std::string const& usage, AggregateUploadThroughputOptions options) {
  auto make_status = [](std::ostringstream& os) {
    auto const code = google::cloud::StatusCode::kInvalidArgument;
    return google::cloud::Status{code, std::move(os).str()};
  };

  if (options.bucket_name.empty()) {
    std::ostringstream os;
    os << "Missing --bucket option\n" << usage << "\n";
    return make_status(os);
  }
  if (options.object_count <= 0) {
    std::ostringstream os;
    os << "Invalid number of objects (" << options.object_count
       << "), check your --object-count option\n";
    return make_status(os);
  }
  if (options.minimum_object_size > options.maximum_object_size) {
    std::ostringstream os;
    os << "Invalid object size range [" << options.minimum_object_size << ","
       << options.maximum_object_size << "], check your --minimum-object-size"
       << " and --maximum-object-size options";
    return make_status(os);
  }
  if (options.thread_count <= 0) {
    std::ostringstream os;
    os << "Invalid number of threads (" << options.thread_count
       << "), check your --thread-count option\n";
    return make_status(os);
  }
  if (options.iteration_count <= 0) {
    std::ostringstream os;
    os << "Invalid number of iterations (" << options.iteration_count
       << "), check your --iteration-count option\n";
    return make_status(os);
  }
  if (options.client_options.get<GrpcNumChannelsOption>() < 0) {
    std::ostringstream os;
    os << "Invalid number of gRPC channels ("
       << options.client_options.get<GrpcNumChannelsOption>()
       << "), check your --grpc-channel-count option\n";
    return make_status(os);
  }
  return options;
}

}  // namespace

google::cloud::StatusOr<AggregateUploadThroughputOptions>
ParseAggregateUploadThroughputOptions(std::vector<std::string> const& argv,
                                      std::string const& description) {
  AggregateUploadThroughputOptions options;
  bool wants_help = false;
  bool wants_description = false;

  std::vector<OptionDescriptor> desc{
      {"--help", "print usage information",
       [&wants_help](std::string const&) { wants_help = true; }},
      {"--description", "print benchmark description",
       [&wants_description](std::string const&) { wants_description = true; }},
      {"--labels", "user-defined labels to tag the results",
       [&options](std::string const& val) { options.labels = val; }},
      {"--bucket-name", "the bucket where the dataset is located",
       [&options](std::string const& val) { options.bucket_name = val; }},
      {"--object-prefix", "the dataset prefix",
       [&options](std::string const& val) { options.object_prefix = val; }},
      {"--object-count", "number of objects created in each iteration",
       [&options](std::string const& val) {
         options.object_count = std::stoi(val);
       }},
      {"--minimum-object-size", "minimum object size for uploads",
       [&options](std::string const& val) {
         options.minimum_object_size = ParseSize(val);
       }},
      {"--maximum-object-size", "maximum object size for uploads",
       [&options](std::string const& val) {
         options.maximum_object_size = ParseSize(val);
       }},
      {"--resumable-upload-chunk-size", "how much data is sent in each chunk",
       [&options](std::string const& val) {
         options.resumable_upload_chunk_size = ParseSize(val);
       }},
      {"--thread-count", "set the number of threads in the benchmark",
       [&options](std::string const& val) {
         options.thread_count = std::stoi(val);
       }},
      {"--iteration-count", "set the number of iterations in the benchmark",
       [&options](std::string const& val) {
         options.iteration_count = std::stoi(val);
       }},
      {"--api", "select the API (JSON, XML, or GRPC) for the benchmark",
       [&options](std::string const& val) { options.api = val; }},
      {"--client-per-thread",
       "use a different storage::Client object in each thread",
       [&options](std::string const& val) {
         options.client_per_thread =
             testing_util::ParseBoolean(val).value_or("true");
       }},
      {"--grpc-channel-count", "controls the number of gRPC channels",
       [&options](std::string const& val) {
         options.client_options.set<GrpcNumChannelsOption>(std::stoi(val));
       }},
      {"--rest-http-version", "change the preferred HTTP version",
       [&options](std::string const& val) {
         options.client_options.set<gcs_ex::HttpVersionOption>(val);
       }},
      {"--rest-endpoint", "change the REST endpoint",
       [&options](std::string const& val) {
         options.client_options.set<gcs::RestEndpointOption>(val);
       }},
      {"--grpc-endpoint", "change the gRPC endpoint",
       [&options](std::string const& val) {
         options.client_options.set<EndpointOption>(val);
       }},
      {"--transfer-stall-timeout",
       "configure `storage::TransferStallTimeoutOption`: the maximum time"
       " allowed for data to 'stall' (make insufficient progress) on all"
       " operations, except for downloads (see --download-stall-timeout)."
       " This option is intended for troubleshooting, most of the time the"
       " value is not expected to change the library performance.",
       [&options](std::string const& val) {
         options.client_options.set<gcs::TransferStallTimeoutOption>(
             ParseDuration(val));
       }},
      {"--transfer-stall-minimum-rate",
       "configure `storage::TransferStallMinimumRateOption`: the transfer"
       " is aborted if the average transfer rate is below this limit for"
       " the period set via `storage::TransferStallTimeoutOption`.",
       [&options](std::string const& val) {
         options.client_options.set<gcs::TransferStallMinimumRateOption>(
             static_cast<std::uint32_t>(ParseBufferSize(val)));
       }},
      {"--grpc-background-threads",
       "change the default number of gRPC background threads",
       [&options](std::string const& val) {
         options.client_options.set<GrpcBackgroundThreadPoolSizeOption>(
             std::stoi(val));
       }},
      {"--rest-pool-size", "set the size of the REST connection pools",
       [&options](std::string const& val) {
         options.client_options.set<gcs::ConnectionPoolSizeOption>(
             std::stoi(val));
       }},
  };
  auto usage = BuildUsage(desc, argv[0]);

  auto unparsed = OptionsParse(desc, argv);
  if (wants_help) {
    std::cout << usage << "\n";
    options.exit_after_parse = true;
    return options;
  }

  if (wants_description) {
    std::cout << description << "\n";
    options.exit_after_parse = true;
    return options;
  }

  if (unparsed.size() != 1) {
    std::ostringstream os;
    os << "Unknown arguments or options: "
       << absl::StrJoin(std::next(unparsed.begin()), unparsed.end(), ", ")
       << "\n"
       << usage << "\n";
    return Status{StatusCode::kInvalidArgument, std::move(os).str()};
  }

  return ValidateOptions(usage, std::move(options));
}

}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google
