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
#include <cctype>
#include <iomanip>
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
BenchmarkSetup::BenchmarkSetup(std::string const& prefix, int& argc,
                               char* argv[])
    : start_time_(FormattedStartTime()),
      notes_(FormattedAnnotations()),
      project_id_(),
      instance_id_(),
      app_profile_id_(),
      table_id_(MakeRandomTableId(prefix)) {
  auto usage = [argv](char const* msg) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <project> <instance> <app_profile_id>"
              << " [thread-count (" << kDefaultThreads << ")]"
              << " [test-duration-seconds (" << kDefaultTestDuration << "min)]"
              << " [table-size (" << kDefaultTableSize << ")]"
              << " [use-embedded-server (false)]" << std::endl;
    google::cloud::internal::ThrowRuntimeError(msg);
  };

  if (argc < 4) {
    usage("too few arguments for program.");
  }

  auto shift = [&argc, &argv]() {
    char* r = argv[1];
    std::copy(argv + 2, argv + argc, argv + 1);
    --argc;
    return r;
  };

  project_id_ = shift();
  instance_id_ = shift();
  app_profile_id_ = shift();

  if (argc == 1) {
    return;
  }
  thread_count_ = std::stoi(shift());

  if (argc == 1) {
    return;
  }
  long seconds = std::stol(shift());
  if (seconds <= 0) {
    usage("test-duration-seconds should be > 0");
  }
  test_duration_ = std::chrono::seconds(seconds);

  if (argc == 1) {
    return;
  }
  table_size_ = std::stol(shift());
  if (table_size_ <= kPopulateShardCount) {
    std::ostringstream os;
    os << "table-size parameter should be > " << kPopulateShardCount;
    usage(os.str().c_str());
  }

  if (argc == 1) {
    return;
  }
  std::string value = shift();
  std::transform(value.begin(), value.end(), value.begin(),
                 [](char x) { return std::tolower(x); });
  use_embedded_server_ = value == "true";
}

}  // namespace benchmarks
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
