// Copyright 2017 Google Inc.
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

#include "google/cloud/bigtable/benchmarks/setup.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/internal/build_info.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/internal/throw_delegate.h"
#include <algorithm>
#include <cctype>
#include <ctime>
#include <iostream>
#include <sstream>

/// Supporting types and functions to implement `BenchmarkSetup`
namespace {
std::string FormattedStartTime() {
  auto start = std::chrono::system_clock::now();
  std::time_t start_c = std::chrono::system_clock::to_time_t(start);
  std::string formatted("YYYY-MM-DDTHH:SS:MMZ");
  auto s = std::strftime(&formatted[0], formatted.size() + 1, "%FT%TZ",
                         std::gmtime(&start_c));
  formatted[s] = '\0';
  return formatted;
}

std::string FormattedAnnotations() {
  std::string notes = google::cloud::bigtable::version_string() + ";" +
                      google::cloud::internal::compiler() + ";" +
                      google::cloud::internal::compiler_flags();
  std::transform(notes.begin(), notes.end(), notes.begin(),
                 [](char c) { return c == '\n' ? ';' : c; });
  return notes;
}

std::string MakeRandomTableId(std::string const& prefix) {
  static std::string const table_id_chars(
      "ABCDEFGHIJLKMNOPQRSTUVWXYZabcdefghijlkmnopqrstuvwxyz0123456789_");
  auto gen = google::cloud::internal::MakeDefaultPRNG();
  return prefix + "-" +
         google::cloud::internal::Sample(
             gen, google::cloud::bigtable::benchmarks::kTableIdRandomLetters,
             table_id_chars);
}
}  // anonymous namespace

namespace google {
namespace cloud {
namespace bigtable {
namespace benchmarks {
BenchmarkSetup::BenchmarkSetup(BenchmarkSetupData setup_data)
    : setup_data_(setup_data) {}

google::cloud::StatusOr<BenchmarkSetup> MakeBenchmarkSetup(
    std::string const& prefix, int& argc, char* argv[]) {
  BenchmarkSetupData setup_data;
  setup_data.start_time = FormattedStartTime();
  setup_data.notes = FormattedAnnotations();
  setup_data.table_id = MakeRandomTableId(prefix);

  // An aggregate cannot have brace initializers for non static data members.
  // So we set the "default" values here.
  setup_data.thread_count = kDefaultThreads;
  setup_data.table_size = kDefaultTableSize;
  setup_data.test_duration = std::chrono::seconds(kDefaultTestDuration * 60);
  setup_data.use_embedded_server = false;
  setup_data.parallel_requests = 10;

  auto usage = [argv](char const* msg) -> google::cloud::Status {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <project> <instance> <app_profile_id>"
              << " [thread-count (" << kDefaultThreads << ")]"
              << " [test-duration-seconds (" << kDefaultTestDuration << "min)]"
              << " [table-size (" << kDefaultTableSize << ")]"
              << " [use-embedded-server (false)]\n";
    return google::cloud::Status{google::cloud::StatusCode::kFailedPrecondition,
                                 msg};
  };

  if (argc < 4) {
    return usage("too few arguments for program.");
  }

  auto shift = [&argc, &argv]() {
    char* r = argv[1];
    std::copy(argv + 2, argv + argc, argv + 1);
    --argc;
    return r;
  };

  setup_data.project_id = shift();
  setup_data.instance_id = shift();
  setup_data.app_profile_id = shift();

  if (argc == 1) {
    return BenchmarkSetup{setup_data};
  }
  setup_data.thread_count = std::stoi(shift());

  if (argc == 1) {
    return BenchmarkSetup{setup_data};
  }
  long seconds = std::stol(shift());
  if (seconds <= 0) {
    return usage("test-duration-seconds should be > 0");
  }
  setup_data.test_duration = std::chrono::seconds(seconds);

  if (argc == 1) {
    return BenchmarkSetup{setup_data};
  }
  setup_data.table_size = std::stol(shift());
  if (setup_data.table_size <= kPopulateShardCount) {
    std::ostringstream os;
    os << "table-size parameter should be > " << kPopulateShardCount;
    return usage(os.str().c_str());
  }

  if (argc == 1) {
    return BenchmarkSetup{setup_data};
  }
  std::string value = shift();
  std::transform(value.begin(), value.end(), value.begin(),
                 [](char x) { return std::tolower(x); });
  setup_data.use_embedded_server = value == "true";

  if (argc == 1) {
    return BenchmarkSetup{setup_data};
  }

  setup_data.parallel_requests = std::stoi(shift());
  return BenchmarkSetup{setup_data};
}

}  // namespace benchmarks
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
