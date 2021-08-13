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
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/internal/throw_delegate.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>

/// Supporting types and functions to implement `BenchmarkSetup`
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

std::string MakeRandomTableId(std::string const& prefix) {
  static auto const* const kTableIdChars = new std::string(
      "ABCDEFGHIJLKMNOPQRSTUVWXYZabcdefghijlkmnopqrstuvwxyz0123456789_");
  auto gen = google::cloud::internal::MakeDefaultPRNG();
  return prefix + "-" +
         google::cloud::internal::Sample(
             gen, google::cloud::bigtable::benchmarks::kTableIdRandomLetters,
             *kTableIdChars);
}

}  // anonymous namespace

namespace google {
namespace cloud {
namespace bigtable {
namespace benchmarks {
BenchmarkSetup::BenchmarkSetup(BenchmarkSetupData setup_data)
    : setup_data_(std::move(setup_data)) {}

google::cloud::StatusOr<BenchmarkSetup> MakeBenchmarkSetup(
    std::string const& prefix, int& argc, char* argv[]) {
  using ::google::cloud::internal::GetEnv;

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

  bool auto_run =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES")
          .value_or("") == "yes";
  if (argc == 1 && auto_run) {
    for (auto const& var : {"GOOGLE_CLOUD_PROJECT",
                            "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID"}) {
      auto const value = GetEnv(var).value_or("");
      if (!value.empty()) continue;
      std::ostringstream os;
      os << "The environment variable " << var << " is not set or empty";
      return google::cloud::Status(google::cloud::StatusCode::kUnknown,
                                   std::move(os).str());
    }
    setup_data.project_id = GetEnv("GOOGLE_CLOUD_PROJECT").value();
    setup_data.instance_id =
        GetEnv("GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID").value();
    setup_data.app_profile_id = "default";
    setup_data.thread_count = 1;
    setup_data.test_duration = std::chrono::seconds(1);
    // Must be > 10,000 or scan_throughput_benchmark crashes on Windows
    setup_data.table_size = 11000;
    return BenchmarkSetup{setup_data};
  }

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
  auto const seconds = std::stol(shift());
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
