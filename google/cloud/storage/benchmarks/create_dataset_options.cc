// Copyright 2021 Google LLC
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

#include "google/cloud/storage/benchmarks/create_dataset_options.h"
#include "google/cloud/internal/getenv.h"

namespace google {
namespace cloud {
namespace storage_benchmarks {
namespace {
char const kDescription[] = R"""(Creates datasets for GCS benchmarks.
)""";
}  // namespace

using ::google::cloud::testing_util::OptionDescriptor;

google::cloud::StatusOr<CreateDatasetOptions> ParseCreateDatasetOptions(
    std::vector<std::string> argv) {
  CreateDatasetOptions options;
  bool wants_help = false;
  bool wants_description = false;
  std::vector<OptionDescriptor> desc{
      {"--help", "print usage information",
       [&wants_help](std::string const&) { wants_help = true; }},
      {"--description", "print benchmark description",
       [&wants_description](std::string const&) { wants_description = true; }},
      {"--bucket-name", "use an existing bucket",
       [&options](std::string const& val) { options.bucket_name = val; }},
      {"--object-prefix", "use this prefix for object names",
       [&options](std::string const& val) { options.object_prefix = val; }},
      {"--object-count", "set the number of objects created by the benchmark",
       [&options](std::string const& val) {
         options.object_count = std::stoi(val);
       }},
      {"--minimum-object-size", "minimum size of the objects in the dataset",
       [&options](std::string const& val) {
         options.minimum_object_size = ParseBufferSize(val);
       }},
      {"--maximum-object-size", "maximum size of the objects in the dataset",
       [&options](std::string const& val) {
         options.maximum_object_size = ParseBufferSize(val);
       }},
      {"--thread-count", "set the number of threads in the benchmark",
       [&options](std::string const& val) {
         options.thread_count = std::stoi(val);
       }},
  };
  auto usage = BuildUsage(desc, argv[0]);

  auto unparsed = OptionsParse(desc, argv);
  if (wants_help) {
    std::cout << usage << "\n";
    options.exit_after_parse = true;
  }

  if (wants_description) {
    std::cout << kDescription << "\n";
    options.exit_after_parse = true;
  }

  if (unparsed.size() > 2) {
    std::ostringstream os;
    os << "Unknown arguments or options\n" << usage << "\n";
    return google::cloud::Status{google::cloud::StatusCode::kInvalidArgument,
                                 std::move(os).str()};
  }
  if (unparsed.size() == 2) {
    options.bucket_name = unparsed[1];
  }
  if (options.bucket_name.empty()) {
    std::ostringstream os;
    os << "Missing value for --bucket_name option" << usage << "\n";
    return google::cloud::Status{google::cloud::StatusCode::kInvalidArgument,
                                 std::move(os).str()};
  }

  if (options.minimum_object_size > options.maximum_object_size) {
    std::ostringstream os;
    os << "Invalid object size range [" << options.minimum_object_size << ","
       << options.maximum_object_size << "), check your --minimum-object-size"
       << " and --maximum-object-size options";
    return google::cloud::Status{google::cloud::StatusCode::kInvalidArgument,
                                 std::move(os).str()};
  }

  if (options.object_count <= 0) {
    std::ostringstream os;
    os << "Invalid object count (" << options.object_count
       << "), check your --object-count option";
    return google::cloud::Status{google::cloud::StatusCode::kInvalidArgument,
                                 std::move(os).str()};
  }

  if (options.thread_count <= 0) {
    std::ostringstream os;
    os << "Invalid thread count (" << options.thread_count
       << "), check your --thread-count option";
    return google::cloud::Status{google::cloud::StatusCode::kInvalidArgument,
                                 std::move(os).str()};
  }

  return options;
}

}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google
