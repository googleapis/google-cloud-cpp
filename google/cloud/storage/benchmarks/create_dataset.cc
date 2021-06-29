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

#include "google/cloud/storage/benchmarks/bounded_queue.h"
#include "google/cloud/storage/benchmarks/create_dataset_options.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/internal/build_info.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include <cstdint>
#include <cstdlib>
#include <future>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace {
namespace gcs = google::cloud::storage;
namespace gcs_bm = google::cloud::storage_benchmarks;
using ::google::cloud::internal::GetEnv;

google::cloud::StatusOr<gcs_bm::CreateDatasetOptions> ParseArgs(int argc,
                                                                char* argv[]) {
  bool auto_run =
      GetEnv("GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES").value_or("") == "yes";
  if (auto_run) {
    auto generator =
        google::cloud::internal::DefaultPRNG(std::random_device{}());
    auto bucket_name =
        GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME").value();
    auto prefix = gcs_bm::MakeRandomObjectName(generator).substr(0, 32) + "/";
    return gcs_bm::ParseCreateDatasetOptions({
        "self-test",
        "--bucket-name=" + bucket_name,
        "--object-prefix=" + prefix,
        "--object-count=5",
        "--thread-count=2",
    });
  }

  return gcs_bm::ParseCreateDatasetOptions({argv, argv + argc});
}

void UploadOneObject(gcs::Client& client, std::string const& bucket_name,
                     std::string const& object_name, std::int64_t object_size,
                     std::string const& block) {
  auto stream =
      client.WriteObject(bucket_name, object_name, gcs::IfGenerationMatch(0));
  for (std::int64_t size = 0; size < object_size;) {
    auto const count =
        (std::min)(static_cast<std::int64_t>(block.size()), object_size - size);
    stream.write(block.data(), static_cast<std::streamsize>(count));
    size += count;
  }
  stream.Close();
}

void CreateObjects(gcs_bm::CreateDatasetOptions const& options,
                   std::seed_seq::result_type seed, std::int64_t object_count) {
  auto client = gcs::Client();
  google::cloud::internal::DefaultPRNG generator(seed);
  auto constexpr kRandomBlockSize = 512 * gcs_bm::kKiB;
  auto block = gcs_bm::MakeRandomData(generator, kRandomBlockSize);
  auto object_size = std::uniform_int_distribution<std::int64_t>(
      options.minimum_object_size, options.maximum_object_size);

  for (std::int64_t i = 0; i != object_count; ++i) {
    auto object_name =
        options.object_prefix + gcs_bm::MakeRandomObjectName(generator);

    UploadOneObject(client, options.bucket_name, object_name,
                    object_size(generator), block);
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  auto options = ParseArgs(argc, argv);
  if (!options) {
    std::cerr << options.status() << "\n";
    return 1;
  }
  if (options->exit_after_parse) return 1;

  auto d = std::div(options->object_count, options->thread_count);
  std::vector<std::int64_t> counts(options->thread_count, d.quot);
  for (auto& c : counts) {
    if (--d.rem < 0) break;
    ++c;
  }

  std::vector<std::seed_seq::result_type> seeds(counts.size());
  std::random_device rd;
  std::seed_seq seq({rd(), rd()});
  seq.generate(seeds.begin(), seeds.end());

  std::string notes = google::cloud::storage::version_string() + ";" +
                      google::cloud::internal::compiler() + ";" +
                      google::cloud::internal::compiler_flags();
  std::transform(notes.begin(), notes.end(), notes.begin(),
                 [](char c) { return c == '\n' ? ';' : c; });

  std::cout << "# Start time: "
            << google::cloud::internal::FormatRfc3339(
                   std::chrono::system_clock::now())
            << "\n# Bucket Name: " << options->bucket_name
            << "\n# Object Prefix: " << options->object_prefix
            << "\n# Object Count: " << options->object_count
            << "\n# Minimum Object Size (MiB): "
            << options->minimum_object_size / gcs_bm::kMiB
            << "\n# Maximum Object Size (MiB): "
            << options->maximum_object_size / gcs_bm::kMiB
            << "\n# Thread Count: " << options->thread_count
            << "\n# Build info: " << notes << std::endl;

  std::vector<std::future<void>> tasks(counts.size());
  for (std::size_t i = 0; i != counts.size(); ++i) {
    tasks[i] = std::async(std::launch::async, CreateObjects, *options, seeds[i],
                          counts[i]);
  }
  for (auto& t : tasks) t.get();

  // If this is just a test, cleanup the objects we just created.
  if (GetEnv("GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES").value_or("") == "yes") {
    gcs_bm::DeleteAllObjects(gcs::Client(), options->bucket_name,
                             gcs::Prefix(options->object_prefix), 2);
  }

  return 0;
}
