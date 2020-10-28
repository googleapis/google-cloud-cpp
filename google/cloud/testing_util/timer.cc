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
int rusage_who() {
#if GOOGLE_CLOUD_CPP_HAVE_RUSAGE_THREAD
  return RUSAGE_THREAD;
#else
  return RUSAGE_SELF;
#endif  // GOOGLE_CLOUD_CPP_HAVE_RUSAGE_THREAD
}
}  // namespace
#endif  // GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE

void Timer::Start() {
#if GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE
  (void)getrusage(rusage_who(), &start_usage_);
#endif  // GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE
  start_ = std::chrono::steady_clock::now();
}

void Timer::Stop() {
  elapsed_time_ = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - start_);

#if GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE
  auto as_usec = [](timeval const& tv) {
    return std::chrono::microseconds(std::chrono::seconds(tv.tv_sec)) +
           std::chrono::microseconds(tv.tv_usec);
  };

  struct rusage now {};
  (void)getrusage(rusage_who(), &now);
  auto utime = as_usec(now.ru_utime) - as_usec(start_usage_.ru_utime);
  auto stime = as_usec(now.ru_stime) - as_usec(start_usage_.ru_stime);
  cpu_time_ = utime + stime;
  double cpu_fraction = 0;
  if (elapsed_time_.count() != 0) {
    cpu_fraction = static_cast<double>(cpu_time_.count()) /
                   static_cast<double>(elapsed_time_.count());
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

bool Timer::SupportPerThreadUsage() {
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
