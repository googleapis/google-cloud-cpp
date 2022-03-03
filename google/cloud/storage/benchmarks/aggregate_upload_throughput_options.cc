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
#include "google/cloud/internal/absl_str_join_quiet.h"
#include <iterator>
#include <sstream>

namespace google {
namespace cloud {
namespace storage_benchmarks {

using ::google::cloud::testing_util::OptionDescriptor;

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
       [&options](std::string const& val) {
         options.api = ParseApiName(val).value();
       }},
      {"--grpc-channel-count", "controls the number of gRPC channels",
       [&options](std::string const& val) {
         options.grpc_channel_count = std::stoi(val);
       }},
      {"--grpc-plugin-config",
       "low-level experimental settings for the GCS+gRPC plugin",
       [&options](std::string const& val) {
         options.grpc_plugin_config = val;
       }},
      {"--rest-http-version", "change the preferred HTTP version",
       [&options](std::string const& val) { options.rest_http_version = val; }},
      {"--client-per-thread",
       "use a different storage::Client object in each thread",
       [&options](std::string const& val) {
         options.client_per_thread =
             testing_util::ParseBoolean(val).value_or("true");
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

  auto make_status = [](std::ostringstream& os) {
    auto const code = google::cloud::StatusCode::kInvalidArgument;
    return google::cloud::Status{code, std::move(os).str()};
  };

  if (unparsed.size() != 1) {
    std::ostringstream os;
    os << "Unknown arguments or options: "
       << absl::StrJoin(std::next(unparsed.begin()), unparsed.end(), ", ")
       << "\n"
       << usage << "\n";
    return make_status(os);
  }
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
  if (options.grpc_channel_count < 0) {
    std::ostringstream os;
    os << "Invalid number of gRPC channels (" << options.grpc_channel_count
       << "), check your --grpc-channel-count option\n";
    return make_status(os);
  }

  return options;
}

}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google
