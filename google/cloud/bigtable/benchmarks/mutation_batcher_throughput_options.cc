// Copyright 2021 Google Inc.
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

#include "google/cloud/bigtable/benchmarks/mutation_batcher_throughput_options.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/command_line_parsing.h"
#include <sstream>

namespace google {
namespace cloud {
namespace bigtable {
namespace benchmarks {

using ::google::cloud::testing_util::BuildUsage;
using ::google::cloud::testing_util::OptionDescriptor;
using ::google::cloud::testing_util::OptionsParse;
using ::google::cloud::testing_util::ParseDuration;

google::cloud::StatusOr<MutationBatcherThroughputOptions>
ParseMutationBatcherThroughputOptions(std::vector<std::string> const& argv,
                                      std::string const& description) {
  MutationBatcherThroughputOptions options;
  bool wants_help = false;
  bool wants_description = false;

  std::vector<OptionDescriptor> desc{
      {"--help", "print usage information",
       [&wants_help](std::string const&) { wants_help = true; }},
      {"--description", "print benchmark description",
       [&wants_description](std::string const&) { wants_description = true; }},
      {"--project-id", "the GCP Project ID",
       [&options](std::string const& val) { options.project_id = val; }},
      {"--instance-id", "the Instance ID",
       [&options](std::string const& val) { options.instance_id = val; }},
      {"--table-id",
       "the benchmark will be run on this table, instead of creating a new "
       "one",
       [&options](std::string const& val) { options.table_id = val; }},
      {"--max-time",
       "the benchmark will be cut off after this many seconds if it is still "
       "running. A value of 0 means no cut off",
       [&options](std::string const& val) {
         options.max_time = ParseDuration(val);
       }},
      {"--shard-count",
       "the number of initial splits provided to the table. The rows will be "
       "uniformly distributed across these shards",
       [&options](std::string const& val) {
         options.shard_count = std::stoi(val);
       }},
      {"--write-thread-count",
       "the number of threads launched to write mutations. The M mutations are "
       "broken up across this many threads. Each thread has its own batcher",
       [&options](std::string const& val) {
         options.write_thread_count = std::stoi(val);
       }},
      {"--batcher-thread-count",
       "the number of background threads running the batcher. These threads "
       "reorganize pending batches when a response is received from the server",
       [&options](std::string const& val) {
         options.batcher_thread_count = std::stoi(val);
       }},
      {"--mutation-count", "the total number of mutations",
       [&options](std::string const& val) {
         options.mutation_count = std::stoll(val);
       }},
      {"--max-batches",
       "the maximum batches that can be outstanding at any time",
       [&options](std::string const& val) {
         options.max_batches = std::stoi(val);
       }},
      {"--batch-size",
       "the maximum mutations that can be packed into one batch",
       [&options](std::string const& val) {
         options.batch_size = std::stoi(val);
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
  if (options.project_id.empty()) {
    std::ostringstream os;
    os << "Missing --project-id option\n" << usage << "\n";
    return make_status(os);
  }
  if (options.instance_id.empty()) {
    std::ostringstream os;
    os << "Missing --instance-id option\n" << usage << "\n";
    return make_status(os);
  }
  if (options.max_time.count() < 0) {
    std::ostringstream os;
    os << "Invalid cutoff time (" << options.max_time.count()
       << "s). Check your --max-time option\n";
    return make_status(os);
  }
  if (options.shard_count <= 0) {
    std::ostringstream os;
    os << "Invalid number of shards (" << options.shard_count
       << "). Check your --shard-count option\n";
    return make_status(os);
  }
  if (options.write_thread_count <= 0) {
    std::ostringstream os;
    os << "Invalid number of write threads (" << options.write_thread_count
       << "). Check your --write-thread-count option\n";
    return make_status(os);
  }
  if (options.batcher_thread_count <= 0) {
    std::ostringstream os;
    os << "Invalid number of batcher threads (" << options.batcher_thread_count
       << "). Check your --batcher-thread-count option\n";
    return make_status(os);
  }
  if (options.mutation_count < 0) {
    std::ostringstream os;
    os << "Invalid number of total mutations (" << options.mutation_count
       << "). Check your --mutation-count option\n";
    return make_status(os);
  }
  if (options.max_batches <= 0) {
    std::ostringstream os;
    os << "Invalid maximum number of outstanding batches("
       << options.max_batches << "). Check your --max-batches option\n";
    return make_status(os);
  }
  // A maximum of 100,000 mutations per request can be sent to the batcher.
  // See `kBigtableMutationLimit`
  if (options.batch_size <= 0 || options.batch_size > 100000) {
    std::ostringstream os;
    os << "Invalid maximum number of mutations per batch("
       << options.max_batches
       << "). This value must fall in the range: [1, 100000]. Check your "
          "--batch-size option\n";
    return make_status(os);
  }

  return options;
}

}  // namespace benchmarks
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
