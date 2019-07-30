// Copyright 2019 Google LLC
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

#include "google/cloud/storage/benchmarks/benchmark_utils.h"
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace google {
namespace cloud {
namespace storage_benchmarks {
std::string MakeRandomBucketName(google::cloud::internal::DefaultPRNG& gen,
                                 std::string const& prefix) {
  // The total length of this bucket name must be <= 63 characters,
  static std::size_t const kMaxBucketNameLength = 63;
  std::size_t const max_random_characters =
      kMaxBucketNameLength - prefix.size();
  return prefix + google::cloud::internal::Sample(
                      gen, static_cast<int>(max_random_characters),
                      "abcdefghijklmnopqrstuvwxyz012456789");
}

std::string MakeRandomObjectName(google::cloud::internal::DefaultPRNG& gen) {
  return google::cloud::internal::Sample(gen, 128,
                                         "abcdefghijklmnopqrstuvwxyz"
                                         "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                         "0123456789");
}

std::string MakeRandomFileName(google::cloud::internal::DefaultPRNG& gen) {
  return google::cloud::internal::Sample(gen, 28,
                                         "abcdefghijklmnopqrstuvwxyz"
                                         "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                         "0123456789") +
         ".txt";
}

std::string MakeRandomData(google::cloud::internal::DefaultPRNG& gen,
                           std::size_t desired_size) {
  std::string result;
  result.reserve(desired_size);

  // Create lines of 128 characters to start with, we can fill the remaining
  // characters at the end.
  constexpr int kLineSize = 128;
  auto gen_random_line = [&gen](std::size_t count) {
    return google::cloud::internal::Sample(gen, static_cast<int>(count - 1),
                                           "abcdefghijklmnopqrstuvwxyz"
                                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                           "012456789"
                                           " - _ : /") +
           "\n";
  };
  while (result.size() + kLineSize < desired_size) {
    result += gen_random_line(kLineSize);
  }
  if (result.size() < desired_size) {
    result += gen_random_line(desired_size - result.size());
  }

  return result;
}

bool EndsWith(std::string const& val, std::string const& suffix) {
  auto pos = val.rfind(suffix);
  if (pos == std::string::npos) {
    return false;
  }
  return pos == val.size() - suffix.size();
}

// This parser does not validate the input fully, but it is good enough for our
// purposes.
std::int64_t ParseSize(std::string const& val) {
  long s = std::stol(val);
  if (EndsWith(val, "TiB")) {
    return s * kTiB;
  }
  if (EndsWith(val, "GiB")) {
    return s * kGiB;
  }
  if (EndsWith(val, "MiB")) {
    return s * kMiB;
  }
  if (EndsWith(val, "KiB")) {
    return s * kKiB;
  }
  if (EndsWith(val, "TB")) {
    return s * kTB;
  }
  if (EndsWith(val, "GB")) {
    return s * kGB;
  }
  if (EndsWith(val, "MB")) {
    return s * kMB;
  }
  if (EndsWith(val, "KB")) {
    return s * kKB;
  }
  return s;
}

// This parser does not validate the input fully, but it is good enough for our
// purposes.
std::chrono::seconds ParseDuration(std::string const& val) {
  long s = std::stol(val);
  if (EndsWith(val, "h")) {
    return s * std::chrono::seconds(std::chrono::hours(1));
  }
  if (EndsWith(val, "m")) {
    return s * std::chrono::seconds(std::chrono::minutes(1));
  }
  if (EndsWith(val, "s")) {
    return s * std::chrono::seconds(std::chrono::seconds(1));
  }
  return std::chrono::seconds(s);
}

google::cloud::optional<bool> ParseBoolean(std::string const& val) {
  if (val.empty()) {
    return {};
  }
  auto lower = val;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](char x) { return static_cast<char>(std::tolower(x)); });
  if (lower == "true") {
    return true;
  } else if (lower == "false") {
    return false;
  }
  return {};
}

std::string Basename(std::string const& path) {
  // With C++17 we would use std::filesytem::path, until then do the poor's
  // person version.
#if _WIN32
  return path.substr(path.find_last_of("\\/") + 1);
#else
  return path.substr(path.find_last_of('/') + 1);
#endif  // _WIN32
}

std::string BuildUsage(std::vector<OptionDescriptor> const& desc,
                       std::string const& command_path) {
  std::ostringstream os;
  os << "Usage: " << Basename(command_path) << " [options] <region>\n";
  for (auto const& d : desc) {
    os << "    " << d.option << ": " << d.help << "\n";
  }
  return std::move(os).str();
}

std::vector<std::string> OptionsParse(std::vector<OptionDescriptor> const& desc,
                                      std::vector<std::string> argv) {
  if (argv.empty()) {
    return argv;
  }
  std::string const usage = BuildUsage(desc, argv[0]);

  auto next_arg = argv.begin() + 1;
  while (next_arg != argv.end()) {
    std::string const& argument = *next_arg;

    // Try to match `argument` against the options in `desc`
    bool matched = false;
    for (auto const& d : desc) {
      if (argument.rfind(d.option, 0) != 0) {
        // Not a match, keep searching
        continue;
      }
      std::string val = argument.substr(d.option.size());
      if (!val.empty() && val[0] != '=') {
        // Matched a prefix of an option, keep searching.
        continue;
      }
      if (!val.empty()) {
        // The first character must be '=', remove it too.
        val.erase(val.begin());
      }
      d.parser(val);
      // This is a match, consume the argument and stop the search.
      matched = true;
      break;
    }
    // If next_arg is matched against any option erase it, otherwise skip it.
    next_arg = matched ? argv.erase(next_arg) : next_arg + 1;
  }
  return argv;
}

#if GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE
namespace {
int rusage_who() {
#if GOOGLE_CLOUD_CPP_HAVE_RUSAGE_THREAD
  return RUSAGE_THREAD;
#else
  return RUSAGE_SELF;
#endif  // GOOGLE_CLOUD_CPP_HAVE_RUSAGE_THREAD
}
}  // namespace
#endif  // GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE

void SimpleTimer::Start() {
#if GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE
  (void)getrusage(rusage_who(), &start_usage_);
#endif  // GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE
  start_ = std::chrono::steady_clock::now();
}

void SimpleTimer::Stop() {
  using namespace std::chrono;
  elapsed_time_ = duration_cast<microseconds>(steady_clock::now() - start_);

#if GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE
  auto as_usec = [](timeval const& tv) {
    return microseconds(seconds(tv.tv_sec)) + microseconds(tv.tv_usec);
  };

  struct rusage now {};
  (void)getrusage(rusage_who(), &now);
  auto utime = as_usec(now.ru_utime) - as_usec(start_usage_.ru_utime);
  auto stime = as_usec(now.ru_stime) - as_usec(start_usage_.ru_stime);
  cpu_time_ = utime + stime;
  double cpu_fraction = 0;
  if (elapsed_time_.count() != 0) {
    cpu_fraction =
        (cpu_time_).count() / static_cast<double>(elapsed_time_.count());
  }
  now.ru_minflt -= start_usage_.ru_minflt;
  now.ru_majflt -= start_usage_.ru_majflt;
  now.ru_nswap -= start_usage_.ru_nswap;
  now.ru_inblock -= start_usage_.ru_inblock;
  now.ru_oublock -= start_usage_.ru_oublock;
  now.ru_msgsnd -= start_usage_.ru_msgsnd;
  now.ru_msgrcv -= start_usage_.ru_msgrcv;
  now.ru_nsignals -= start_usage_.ru_nsignals;
  now.ru_nvcsw -= start_usage_.ru_nvcsw;
  now.ru_nivcsw -= start_usage_.ru_nivcsw;

  std::ostringstream os;
  os << "# user time                    =" << utime.count() << " us\n"
     << "# system time                  =" << stime.count() << " us\n"
     << "# CPU fraction                 =" << cpu_fraction << "\n"
     << "# maximum resident set size    =" << now.ru_maxrss << " KiB\n"
     << "# integral shared memory size  =" << now.ru_ixrss << " KiB\n"
     << "# integral unshared data size  =" << now.ru_idrss << " KiB\n"
     << "# integral unshared stack size =" << now.ru_isrss << " KiB\n"
     << "# soft page faults             =" << now.ru_minflt << "\n"
     << "# hard page faults             =" << now.ru_majflt << "\n"
     << "# swaps                        =" << now.ru_nswap << "\n"
     << "# block input operations       =" << now.ru_inblock << "\n"
     << "# block output operations      =" << now.ru_oublock << "\n"
     << "# IPC messages sent            =" << now.ru_msgsnd << "\n"
     << "# IPC messages received        =" << now.ru_msgrcv << "\n"
     << "# signals received             =" << now.ru_nsignals << "\n"
     << "# voluntary context switches   =" << now.ru_nvcsw << "\n"
     << "# involuntary context switches =" << now.ru_nivcsw << "\n";
  annotations_ = std::move(os).str();
#endif  // GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE
}

bool SimpleTimer::SupportPerThreadUsage() {
#if GOOGLE_CLOUD_CPP_HAVE_RUSAGE_THREAD
  return true;
#else
  return false;
#endif  // GOOGLE_CLOUD_CPP_HAVE_RUSAGE_THREAD
}

}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google
