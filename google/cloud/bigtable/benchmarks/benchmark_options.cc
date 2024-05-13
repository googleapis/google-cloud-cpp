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

#include "google/cloud/bigtable/benchmarks/benchmark_options.h"
#include "google/cloud/bigtable/testing/random_names.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/build_info.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/command_line_parsing.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include <sstream>

namespace {
std::string FormattedStartTime() {
  return absl::FormatTime("%FT%TZ", absl::Now(), absl::UTCTimeZone());
}

std::string FormattedAnnotations() {
  std::string notes = google::cloud::bigtable::version_string() + ";" +
                      google::cloud::internal::compiler() + ";" +
                      google::cloud::internal::compiler_flags();
  std::transform(notes.begin(), notes.end(), notes.begin(),
                 [](char c) { return c == '\n' ? ';' : c; });
  return notes;
}

}  // anonymous namespace

namespace google {
namespace cloud {
namespace bigtable {
namespace benchmarks {

using ::google::cloud::bigtable::testing::RandomTableId;
using ::google::cloud::testing_util::BuildUsage;
using ::google::cloud::testing_util::OptionDescriptor;
using ::google::cloud::testing_util::OptionsParse;
using ::google::cloud::testing_util::ParseBoolean;
using ::google::cloud::testing_util::ParseDuration;

google::cloud::StatusOr<BenchmarkOptions> ParseBenchmarkOptions(
    std::vector<std::string> const& argv, std::string const& description) {
  BenchmarkOptions options;
  bool wants_help = false;
  bool wants_description = false;

  options.start_time = FormattedStartTime();
  options.notes = FormattedAnnotations();
  auto generator = google::cloud::internal::MakeDefaultPRNG();
  options.table_id = RandomTableId(generator);

  std::vector<OptionDescriptor> desc{
      {"--help", "print usage information",
       [&wants_help](std::string const&) { wants_help = true; }},
      {"--description", "print benchmark description",
       [&wants_description](std::string const&) { wants_description = true; }},
      {"--project-id", "the GCP Project ID",
       [&options](std::string const& val) { options.project_id = val; }},
      {"--instance-id", "the Instance ID",
       [&options](std::string const& val) { options.instance_id = val; }},
      {"--app-profile-id", "the Application Profile ID",
       [&options](std::string const& val) { options.app_profile_id = val; }},
      {"--thread-count", "how many threads to split the tasks across",
       [&options](std::string const& val) {
         options.thread_count = std::stoi(val);
       }},
      {"--test-duration", "how long to run the benchmarks",
       [&options](std::string const& val) {
         options.test_duration = ParseDuration(val);
       }},
      {"--table-size", "the number of rows in the table",
       [&options](std::string const& val) {
         options.table_size = std::stol(val);
       }},
      {"--use-embedded-server", "whether to use the embedded Bigtable server",
       [&options](std::string const& val) {
         options.use_embedded_server = ParseBoolean(val).value_or(true);
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

  auto make_status = [](std::ostringstream& os,
                        google::cloud::internal::ErrorInfoBuilder info) {
    return google::cloud::internal::InvalidArgumentError(std::move(os).str(),
                                                         std::move(info));
  };

  if (unparsed.size() != 1) {
    std::ostringstream os;
    os << "Unknown arguments or options: "
       << absl::StrJoin(std::next(unparsed.begin()), unparsed.end(), ", ")
       << "\n"
       << usage << "\n";
    return make_status(os, GCP_ERROR_INFO());
  }
  if (options.project_id.empty()) {
    std::ostringstream os;
    os << "Missing --project-id option\n" << usage << "\n";
    return make_status(os, GCP_ERROR_INFO());
  }
  if (options.instance_id.empty()) {
    std::ostringstream os;
    os << "Missing --instance-id option\n" << usage << "\n";
    return make_status(os, GCP_ERROR_INFO());
  }
  if (options.app_profile_id.empty()) {
    std::ostringstream os;
    os << "Missing --app-profile-id option\n" << usage << "\n";
    return make_status(os, GCP_ERROR_INFO());
  }
  if (options.thread_count <= 0) {
    std::ostringstream os;
    os << "Invalid number of threads (" << options.thread_count
       << "). Check your --thread-count option\n";
    return make_status(os, GCP_ERROR_INFO());
  }
  if (options.table_size <= kPopulateShardCount) {
    std::ostringstream os;
    os << "Invalid table size (" << options.table_size
       << "). This value must be greater than " << kPopulateShardCount
       << ". Check your --table-size option\n";
    return make_status(os, GCP_ERROR_INFO());
  }
  if (options.test_duration.count() <= 0) {
    std::ostringstream os;
    os << "Invalid test duration seconds (" << options.test_duration.count()
       << "). Check your --test-duration option.\n";
    return make_status(os, GCP_ERROR_INFO());
  }
  return options;
}

}  // namespace benchmarks
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
