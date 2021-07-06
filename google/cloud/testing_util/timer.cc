// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/testing_util/timer.h"
#include <sstream>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {

#if GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE
namespace {
int rusage_who(CpuAccounting accounting) {
#if GOOGLE_CLOUD_CPP_HAVE_RUSAGE_THREAD
  return accounting == CpuAccounting::kPerThread ? RUSAGE_THREAD : RUSAGE_SELF;
#else
  (void)accounting;
  return RUSAGE_SELF;
#endif  // GOOGLE_CLOUD_CPP_HAVE_RUSAGE_THREAD
}
}  // namespace
#endif  // GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE

Timer::Timer(CpuAccounting accounting)
    : accounting_(accounting), start_(std::chrono::steady_clock::now()) {
#if GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE
  (void)getrusage(rusage_who(accounting_), &start_usage_);
#endif  // GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE
}

Timer::Snapshot Timer::Sample() const {
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - start_);
#if !GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE
  return Snapshot{elapsed, std::chrono::microseconds()};
#else
  auto as_usec = [](timeval const& tv) {
    return std::chrono::microseconds(std::chrono::seconds(tv.tv_sec)) +
           std::chrono::microseconds(tv.tv_usec);
  };

  struct rusage now {};
  (void)getrusage(rusage_who(accounting_), &now);
  auto const utime = as_usec(now.ru_utime) - as_usec(start_usage_.ru_utime);
  auto const stime = as_usec(now.ru_stime) - as_usec(start_usage_.ru_stime);
  return Snapshot{std::move(elapsed), utime + stime};
#endif  // GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE
}

std::string Timer::Annotations() const {
#if !GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE
  return "# No usage annotations are available";
#else
  auto usage = Sample();

  auto as_usec = [](timeval const& tv) {
    return std::chrono::microseconds(std::chrono::seconds(tv.tv_sec)) +
           std::chrono::microseconds(tv.tv_usec);
  };

  struct rusage now {};
  (void)getrusage(rusage_who(accounting_), &now);
  auto utime = as_usec(now.ru_utime) - as_usec(start_usage_.ru_utime);
  auto stime = as_usec(now.ru_stime) - as_usec(start_usage_.ru_stime);
  double cpu_fraction = 0;
  if (usage.elapsed_time.count() != 0) {
    cpu_fraction = static_cast<double>(usage.cpu_time.count()) /
                   static_cast<double>(usage.elapsed_time.count());
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
     << "# involuntary context switches =" << now.ru_nivcsw;
  return std::move(os).str();
#endif  // GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE
}

bool Timer::SupportsPerThreadUsage() {
#if GOOGLE_CLOUD_CPP_HAVE_RUSAGE_THREAD
  return true;
#else
  return false;
#endif  // GOOGLE_CLOUD_CPP_HAVE_RUSAGE_THREAD
}

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
