// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/compute/disks/v1/disks_client.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/example_driver.h"
#include "absl/strings/str_split.h"
#include <map>
#include <regex>
#include <string>
#include <vector>

namespace {

std::map<std::string, std::string> SplitLabelsString(std::string const& arg) {
  std::map<std::string, std::string> labels;
  if (arg.empty()) return labels;
  static auto const* const kInputCheck = new std::regex("(.*:.*)(,.*:.*)*");
  std::smatch match;
  if (!std::regex_match(arg, *kInputCheck)) {
    throw google::cloud::testing_util::Usage{
        "labels should be in the format "
        "\"<label1>:<value1>,<label2>:<value2>,...\""};
  }
  std::vector<std::string> pairs = absl::StrSplit(arg, ',');
  std::transform(pairs.begin(), pairs.end(),
                 std::inserter(labels, labels.end()), [](std::string const& p) {
                   std::pair<std::string, std::string> l =
                       absl::StrSplit(p, ':');
                   return l;
                 });
  return labels;
}

void CreateEmptyDisk(std::vector<std::string> const& argv) {
  if (argv.size() != 4 && argv.size() != 5) {
    throw google::cloud::testing_util::Usage{
        "compute-disk-create-empty-disk <project-id> <zone> <disk-name> "
        "<disk-size-gb> [<label:value>,...]"};
  }
  //! [START compute_disk_create_empty_disk]
  namespace compute_disks = ::google::cloud::compute_disks_v1;
  namespace compute_proto = ::google::cloud::cpp::compute::v1;
  using ::google::cloud::make_ready_future;
  using ::google::cloud::StatusOr;

  [](std::string const& project_id, std::string const& zone,
     std::string const& disk_name, std::string const& disk_size_gb,
     std::map<std::string, std::string> const& labels) {
    auto client =
        compute_disks::DisksClient(compute_disks::MakeDisksConnectionRest());
    compute_proto::Disk disk;
    disk.set_name(disk_name);
    disk.set_size_gb(disk_size_gb);
    for (auto const& l : labels) {
      (*disk.mutable_labels())[l.first] = l.second;
    }
    StatusOr<compute_proto::Disk> result =
        client.InsertDisk(project_id, zone, disk)
            .then([client, project_id, zone, disk_name](auto f) mutable {
              StatusOr<compute_proto::Operation> create_result = f.get();
              if (!create_result) {
                return StatusOr<compute_proto::Disk>(
                    std::move(create_result).status());
              }
              auto get_disk = client.GetDisk(project_id, zone, disk_name);
              return get_disk;
            })
            .get();
    if (!result) throw std::move(result).status();
    std::cout << "Created disk: " << result->DebugString() << "\n";
  }
  //! [END compute_disk_create_empty_disk]
  (argv.at(0), argv.at(1), argv.at(2), argv.at(3),
   ((argv.size() == 5) ? SplitLabelsString(argv.at(4))
                       : std::map<std::string, std::string>{}));
}

void DeleteDisk(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw google::cloud::testing_util::Usage{
        "compute-disk-delete-disk <project-id> <zone> <disk-name>"};
  }
  //! [START compute_disk_delete]
  namespace compute_disks = ::google::cloud::compute_disks_v1;
  [](std::string const& project_id, std::string const& zone,
     std::string const& disk_name) {
    auto client =
        compute_disks::DisksClient(compute_disks::MakeDisksConnectionRest());
    auto delete_result = client.DeleteDisk(project_id, zone, disk_name).get();
    if (!delete_result) throw std::move(delete_result).status();
    std::cout << "Deleted disk: " << disk_name << "\n";
  }
  //! [END compute_disk_delete]
  (argv.at(0), argv.at(1), argv.at(2));
}

void AutoRun(std::vector<std::string> const& argv) {
  using ::google::cloud::internal::GetEnv;
  namespace examples = ::google::cloud::testing_util;

  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet(
      {"GOOGLE_CLOUD_PROJECT", "GOOGLE_CLOUD_CPP_TEST_ZONE"});
  auto const project_id = GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const zone = GetEnv("GOOGLE_CLOUD_CPP_TEST_ZONE").value();
  std::string disk_name = [&] {
    return absl::StrCat(
        "int-test-disk-",
        google::cloud::internal::Sample(
            generator, 8, "abcdefghijklmnopqrstuvwxyz0123456789"));
  }();

  std::cout << "\nRunning CreateEmptyDisk() example" << std::endl;
  CreateEmptyDisk({project_id, zone, disk_name, "10", "sample:sample"});
  std::cout << "\nRunning DeleteDisk() example" << std::endl;
  DeleteDisk({project_id, zone, disk_name});

  std::cout << "\nAutoRun done" << std::endl;
}

}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  google::cloud::testing_util::Example example({
      {"compute-disk-create-empty-disk", CreateEmptyDisk},
      {"compute-disk-delete-disk", DeleteDisk},
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}
