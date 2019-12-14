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

#ifdef __linux__

#include "google/cloud/internal/build_info.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/storage/benchmarks/benchmark_utils.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/parallel_upload.h"
#include "google/cloud/terminate_handler.h"
#include <cstdlib>
#include <fcntl.h>
#include <future>
#include <iomanip>
#include <sstream>
#include <unistd.h>

namespace {
namespace gcs = google::cloud::storage;
namespace gcs_bm = google::cloud::storage_benchmarks;
using google::cloud::Status;
using google::cloud::StatusCode;
using google::cloud::StatusOr;

struct Options {
  std::string project_id;
  std::string bucket;
};

class StatsCalc {
 public:
  StatsCalc()
      : num_(),
        min_(std::numeric_limits<std::size_t>::max()),
        max_(),
        sum_(),
        sum_squares_() {}

  void AddSample(std::size_t s) {
    ++num_;
    max_ = std::max(max_, s);
    min_ = std::min(min_, s);
    sum_ += s;
    sum_squares_ += s * s;
  }

  std::size_t Avg() { return sum_ / num_; }

  std::size_t StdDev() {
    std::size_t avg = Avg();
    return std::sqrt(sum_squares_ / num_ - avg * avg);
  }

  std::size_t num() { return num_; }
  std::size_t min() { return min_; }
  std::size_t max() { return max_; }

 private:
  std::size_t num_;
  std::size_t min_;
  std::size_t max_;
  std::uintmax_t sum_;
  std::uintmax_t sum_squares_;
};

class ScopedFd {
 public:
  ScopedFd(int fd) : fd_(fd) {}
  ~ScopedFd() {
    if (fd_ >= 0) {
      close(fd_);
    }
  }

  int operator*() { return fd_; }

 private:
  int fd_;
};

int MksTemp(std::string& name, std::string const& templ) {
  std::vector<char> buf(templ.begin(), templ.end());
  buf.emplace_back(0);
  int res = mkstemp(buf.data());
  name = std::string(buf.data());
  return res;
}

class TempMemFile {
 public:
  explicit TempMemFile()
      : name_(),
        out_fd_(MksTemp(name_, "/dev/shm/parallel_uploads_bm.XXXXXX")) {
    if (*out_fd_ < 0) {
      google::cloud::Terminate(("creating a temporary file failed with " +
                                std::to_string(errno) + ": " +
                                std::strerror(errno))
                                   .c_str());
    }
  }

  Status Fill(std::uintmax_t file_size) {
    int res = ftruncate(*out_fd_, 0);
    if (res < 0) {
      return Status(StatusCode::kInternal,
                    "Truncating " + name_ + " failed with " +
                        std::to_string(errno) + ": " + std::strerror(errno));
    }
    res = lseek(*out_fd_, 0, SEEK_SET);
    if (res < 0) {
      return Status(StatusCode::kInternal,
                    "Seeking to the beginning of " + name_ + " failed with " +
                        std::to_string(errno) + ": " + std::strerror(errno));
    }
    ScopedFd in_fd(open("/dev/urandom", O_RDONLY));
    if (*in_fd < 0) {
      return Status(StatusCode::kInternal, "opening /dev/urandom failed with " +
                                               std::to_string(errno) + ": " +
                                               std::strerror(errno));
    }

    std::size_t buf_size = 1048576;
    std::unique_ptr<char[]> buf(new char[buf_size]);
    while (file_size > 0) {
      std::size_t to_copy = std::min<std::uintmax_t>(buf_size, file_size);
      ssize_t num_read = TEMP_FAILURE_RETRY(read(*in_fd, buf.get(), to_copy));
      if (num_read < 0) {
        return Status(StatusCode::kInternal,
                      "Reading from /dev/urandom failed with " +
                          std::to_string(errno) + ": " + std::strerror(errno));
      }
      if (num_read == 0) {
        return Status(StatusCode::kInternal, "EOF from /dev/urandom");
      }
      std::size_t off_in_buf = 0;
      file_size -= num_read;
      while (num_read > 0) {
        ssize_t written = TEMP_FAILURE_RETRY(
            write(*out_fd_, buf.get() + off_in_buf, num_read));
        if (written < 0) {
          return Status(StatusCode::kInternal, "Writing to " + name_ +
                                                   " failed with " +
                                                   std::to_string(errno) +
                                                   ": " + std::strerror(errno));
        }
        if (written == 0) {
          return Status(StatusCode::kInternal, "EOF when writing to " + name_);
        }
        off_in_buf += written;
        num_read -= written;
      }
    }
    return Status();
  }

  TempMemFile(TempMemFile const&) = delete;
  TempMemFile& operator=(TempMemFile const&) = delete;
  TempMemFile& operator=(TempMemFile&&) = delete;

  ~TempMemFile() { std::remove(name_.c_str()); }

  std::string name() { return name_; }

 private:
  std::string name_;
  ScopedFd out_fd_;
};

Status PerformUpload(gcs::Client& client, std::string const& file_name,
                     std::string const& bucket_name, std::string const& prefix,
                     std::size_t num_shards) {
  if (num_shards == 1) {
    auto res = client.UploadFile(file_name, bucket_name, prefix + ".dest");
    if (!res) {
      return res.status();
    }
    return Status();
  }
  auto object_metadata = gcs::ParallelUploadFile(
      client, file_name, bucket_name, prefix + ".dest", prefix, false,
      gcs::MinStreamSize(0), gcs::MaxStreams(num_shards));
  if (!object_metadata) {
    return object_metadata.status();
  }

  return Status();
}

StatusOr<std::chrono::milliseconds> TimeSingleUpload(
    gcs::Client& client, std::string const& bucket_name, std::size_t num_shards,
    std::string const& file_name) {
  using std::chrono::milliseconds;

  std::string const& prefix =
      gcs::CreateRandomPrefixName("parallel_upload_bm.");

  auto start = std::chrono::steady_clock::now();
  auto status =
      PerformUpload(client, file_name, bucket_name, prefix, num_shards);
  auto elapsed = std::chrono::steady_clock::now() - start;
  if (status.ok()) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
  }
  return status;
}

StatusOr<StatsCalc> TimeUpload(gcs::Client& client,
                               std::string const& bucket_name,
                               std::size_t num_shards, std::uintmax_t file_size,
                               std::size_t num_samples) {
  StatsCalc calc;
  TempMemFile file;
  Status status = file.Fill(file_size);
  if (!status.ok()) {
    return status;
  }
  for (std::size_t i = 0; i < num_samples; ++i) {
    auto time = TimeSingleUpload(client, bucket_name, num_shards, file.name());
    if (!time) {
      if (calc.num() == 0) {
        return time.status();
      }
      // If it succeeded at least once, let it work.
      continue;
    }
    calc.AddSample(time->count());
  }
  return calc;
}

google::cloud::StatusOr<Options> ParseArgs(int argc, char* argv[]) {
  Options options;
  bool wants_help = false;
  std::vector<gcs_bm::OptionDescriptor> desc{
      {"--help", "print usage information",
       [&wants_help](std::string const&) { wants_help = true; }},
      {"--project-id", "use the given project id for the benchmark",
       [&options](std::string const& val) { options.project_id = val; }},
      {"--bucket", "use the given bucket for the benchmark",
       [&options](std::string const& val) { options.bucket = val; }}};
  auto usage = gcs_bm::BuildUsage(desc, argv[0]);

  auto unparsed = gcs_bm::OptionsParse(desc, {argv, argv + argc});
  if (wants_help) {
    std::cout << usage << "\n";
  }

  if (unparsed.size() > 1) {
    std::ostringstream os;
    os << "Unknown arguments or options (";
    std::copy(unparsed.begin(), unparsed.end(),
              std::ostream_iterator<std::string>(os, " "));
    os << ")\n" << usage << "\n";
    return google::cloud::Status{google::cloud::StatusCode::kInvalidArgument,
                                 std::move(os).str()};
  }
  if (options.project_id.empty()) {
    std::ostringstream os;
    os << "Missing value for --project_id option" << usage << "\n";
    return google::cloud::Status{google::cloud::StatusCode::kInvalidArgument,
                                 std::move(os).str()};
  }
  if (options.bucket.empty()) {
    std::ostringstream os;
    os << "Missing value for --bucket option" << usage << "\n";
    return google::cloud::Status{google::cloud::StatusCode::kInvalidArgument,
                                 std::move(os).str()};
  }

  return options;
}

}  // namespace

int main(int argc, char* argv[]) {
  google::cloud::StatusOr<Options> options = ParseArgs(argc, argv);
  if (!options) {
    std::cerr << options.status() << "\n";
    return 1;
  }

  google::cloud::StatusOr<gcs::ClientOptions> client_options =
      gcs::ClientOptions::CreateDefaultClientOptions();
  if (!client_options) {
    std::cerr << "Could not create ClientOptions, status="
              << client_options.status() << "\n";
    return 1;
  }
  client_options->set_connection_pool_size(0);
  client_options->set_project_id(options->project_id);
  gcs::Client client(*std::move(client_options));

  std::cout << "# Running test on bucket: " << options->bucket << "\n";
  std::string notes = google::cloud::storage::version_string() + ";" +
                      google::cloud::internal::compiler() + ";" +
                      google::cloud::internal::compiler_flags();
  std::transform(notes.begin(), notes.end(), notes.begin(),
                 [](char c) { return c == '\n' ? ';' : c; });
  std::cout << "# Start time: "
            << google::cloud::internal::FormatRfc3339(
                   std::chrono::system_clock::now())
            << "\n# Build info: " << notes << "\n";

  for (std::size_t file_size :
       {1ULL << 25, 1ULL << 27, 1ULL << 30, 1ULL << 32}) {
    for (std::size_t num_shards : {1, 4, 16, 64, 128}) {
      auto calc = TimeUpload(client, options->bucket, num_shards, file_size, 8);
      if (!calc) {
        std::cerr << "Failed to measure file_size=" << file_size
                  << " num_shards=" << num_shards
                  << " status: " << calc.status() << "\n";
        continue;
      }
      std::cout << "Results for file_size=" << file_size
                << " num_shards=" << num_shards << " avg=" << calc->Avg()
                << "ms stddev=" << calc->StdDev() << "ms min=" << calc->min()
                << "ms max=" << calc->max() << "ms avg_bw="
                << ((calc->Avg() > 0)
                        ? (file_size * 1000 / calc->Avg() / 1024 / 1024)
                        : 999999999)
                << "MB/s num=" << calc->num() << "\n";
    }
  }

  return 0;
}
#else   // __linux__
int main() { return 1; }
#endif  // __linux__
