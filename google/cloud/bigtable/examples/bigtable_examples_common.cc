// Copyright 2020 Google LLC
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

#include "google/cloud/bigtable/examples/bigtable_examples_common.h"
#include "google/cloud/bigtable/table_admin.h"
#include "google/cloud/internal/getenv.h"
#include <google/protobuf/util/time_util.h>
#include <sstream>

namespace google {
namespace cloud {
namespace bigtable {
namespace examples {

std::string TablePrefix(std::string const& prefix,
                        std::chrono::system_clock::time_point tp) {
  auto as_seconds =
      std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch());
  return prefix + std::to_string(as_seconds.count()) + "-";
}

std::string RandomTableId(std::string const& prefix,
                          google::cloud::internal::DefaultPRNG& generator) {
  return TablePrefix(prefix, std::chrono::system_clock::now()) +
         google::cloud::internal::Sample(generator, 8,
                                         "abcdefghijklmnopqrstuvwxyz");
}

void CleanupOldTables(std::string const& prefix,
                      google::cloud::bigtable::TableAdmin admin) {
  namespace cbt = google::cloud::bigtable;

  auto const threshold =
      std::chrono::system_clock::now() - std::chrono::hours(48);
  auto const max_table_id = TablePrefix(prefix, threshold);
  auto const max_table_name = admin.TableName(max_table_id);
  auto const prefix_name = admin.TableName(prefix);

  auto tables = admin.ListTables(cbt::TableAdmin::NAME_ONLY);
  if (!tables) return;
  for (auto const& t : *tables) {
    if (t.name().rfind(prefix_name, 0) != 0) continue;
    // Eventually (I heard from good authority around year 2286) the date
    // formatted in seconds will gain an extra digit and this will no longer
    // work. If you are a programmer from the future, I (coryan) am (a) almost
    // certainly dead, (b) very confused that this code is still being
    // maintained or used, and (c) a bit sorry that this caused you problems.
    if (t.name() >= max_table_name) continue;
    // Failure to cleanup is not an error.
    auto const table_id = t.name().substr(t.name().find_last_of('/') + 1);
    std::cout << "Deleting table " << table_id << std::endl;
    (void)admin.DeleteTable(table_id);
  }
}

void CleanupOldBackups(std::string const& cluster_id,
                       google::cloud::bigtable::TableAdmin admin) {
  std::string expire_time = google::protobuf::util::TimeUtil::ToString(
      google::protobuf::util::TimeUtil::GetCurrentTime() -
      google::protobuf::util::TimeUtil::HoursToDuration(24 * 7));
  std::string filter = "expire_time < " + expire_time;
  auto params = google::cloud::bigtable::TableAdmin::ListBackupsParams()
                    .set_cluster(cluster_id)
                    .set_filter(filter);
  auto backups = admin.ListBackups(params);
  if (!backups) return;
  for (auto const& backup : *backups) {
    std::cout << "Deleting backup " << backup.name() << "in cluster "
              << cluster_id << std::endl;
    (void)admin.DeleteBackup(backup);
  }
}

std::string InstancePrefix(std::string const& prefix,
                           std::chrono::system_clock::time_point tp) {
  auto as_seconds =
      std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch());
  return prefix + std::to_string(as_seconds.count()) + "-";
}

auto constexpr kMaxClusterIdLength = 30;

std::string RandomInstanceId(std::string const& prefix,
                             google::cloud::internal::DefaultPRNG& generator) {
  // Cloud Bigtable instance ids must have at least 6 characters, and can have
  // up to 33 characters. But many of the examples append `-c1` or `-c2` to
  // create cluster ids based on the instance id. So we make the generated ids
  // even shorter.
  auto constexpr kMaxInstanceIdLength = kMaxClusterIdLength - 3;
  auto const timestamped_prefix =
      InstancePrefix(prefix, std::chrono::system_clock::now());
  if (timestamped_prefix.size() >= kMaxInstanceIdLength) {
    throw std::invalid_argument("prefix too long for RandomInstanceId()");
  }
  auto const suffix_len = kMaxInstanceIdLength - timestamped_prefix.size();
  return timestamped_prefix + google::cloud::internal::Sample(
                                  generator, static_cast<int>(suffix_len),
                                  "abcdefghijklmnopqrstuvwxyz0123456789");
}

std::string RandomClusterId(std::string const& prefix,
                            google::cloud::internal::DefaultPRNG& generator) {
  if (prefix.size() >= kMaxClusterIdLength) {
    throw std::invalid_argument("prefix too long for RandomClusterId()");
  }
  auto const suffix_len = kMaxClusterIdLength - prefix.size();
  return prefix + google::cloud::internal::Sample(
                      generator, static_cast<int>(suffix_len),
                      "abcdefghijklmnopqrstuvwxyz0123456789");
}

void CleanupOldInstances(std::string const& prefix,
                         google::cloud::bigtable::InstanceAdmin admin) {
  auto const threshold =
      std::chrono::system_clock::now() - std::chrono::hours(48);
  auto const max_instance_id = InstancePrefix(prefix, threshold);
  auto const max_instance_name = admin.InstanceName(max_instance_id);
  auto const prefix_name = admin.InstanceName(prefix);

  auto instances = admin.ListInstances();
  if (!instances) return;
  for (auto const& i : instances->instances) {
    if (i.name().rfind(prefix_name, 0) != 0) continue;
    // Eventually (I heard from good authority around year 2286) the date
    // formatted in seconds will gain an extra digit and this will no longer
    // work. If you are a programmer from the future, I (coryan) am (a) almost
    // certainly dead, (b) very confused that this code is still being
    // maintained or used, and (c) a bit sorry that this caused you problems.
    if (i.name() >= max_instance_name) continue;
    // Failure to cleanup is not an error.
    auto const instance_id = i.name().substr(i.name().find_last_of('/') + 1);
    std::cout << "Deleting instance " << instance_id << std::endl;
    (void)admin.DeleteInstance(instance_id);
  }
}

bool UsingEmulator() {
  return !google::cloud::internal::GetEnv("BIGTABLE_EMULATOR_HOST")
              .value_or("")
              .empty();
}

bool RunAdminIntegrationTests() {
  // When using the emulator we can always run the admin integration tests
  if (UsingEmulator()) return true;

  // In production, we run the admin integration tests only on the nightly
  // builds to stay below the quota limits. Only this build should set the
  // following environment variable.
  return google::cloud::internal::GetEnv(
             "ENABLE_BIGTABLE_ADMIN_INTEGRATION_TESTS")
             .value_or("") == "yes";
}

google::cloud::bigtable::examples::Commands::value_type MakeCommandEntry(
    std::string const& name, std::vector<std::string> const& args,
    TableCommandType const& function) {
  auto command = [=](std::vector<std::string> argv) {
    if ((argv.size() == 1 && argv[0] == "--help") ||
        argv.size() != 3 + args.size()) {
      std::ostringstream os;
      os << name << " <project-id> <instance-id> <table-id>";
      char const* sep = " ";
      for (auto const& a : args) {
        os << sep << a;
      }
      throw Usage{std::move(os).str()};
    }
    google::cloud::bigtable::Table table(
        google::cloud::bigtable::CreateDefaultDataClient(
            argv[0], argv[1], google::cloud::bigtable::ClientOptions()),
        argv[2]);
    argv.erase(argv.begin(), argv.begin() + 3);
    function(table, argv);
  };
  return {name, command};
}

Commands::value_type MakeCommandEntry(std::string const& name,
                                      std::vector<std::string> const& args,
                                      TableAdminCommandType const& function) {
  auto command = [=](std::vector<std::string> argv) {
    auto constexpr kFixedArguments = 2;
    if ((argv.size() == 1 && argv[0] == "--help") ||
        argv.size() != args.size() + kFixedArguments) {
      std::ostringstream os;
      os << name << " <project-id> <instance-id>";
      char const* sep = " ";
      for (auto const& a : args) {
        os << sep << a;
      }
      throw Usage{std::move(os).str()};
    }
    google::cloud::bigtable::TableAdmin table(
        google::cloud::bigtable::CreateDefaultAdminClient(
            argv[0], google::cloud::bigtable::ClientOptions()),
        argv[1]);
    argv.erase(argv.begin(), argv.begin() + kFixedArguments);
    function(table, argv);
  };
  return {name, command};
}

Commands::value_type MakeCommandEntry(
    std::string const& name, std::vector<std::string> const& args,
    InstanceAdminCommandType const& function) {
  auto command = [=](std::vector<std::string> argv) {
    auto constexpr kFixedArguments = 1;
    if ((argv.size() == 1 && argv[0] == "--help") ||
        argv.size() != args.size() + kFixedArguments) {
      std::ostringstream os;
      os << name << " <project-id>";
      char const* sep = " ";
      for (auto const& a : args) {
        os << sep << a;
      }
      throw Usage{std::move(os).str()};
    }
    google::cloud::bigtable::InstanceAdmin instance(
        google::cloud::bigtable::CreateDefaultInstanceAdminClient(
            argv[0], google::cloud::bigtable::ClientOptions()));
    argv.erase(argv.begin(), argv.begin() + kFixedArguments);
    function(instance, argv);
  };
  return {name, command};
}

Commands::value_type MakeCommandEntry(std::string const& name,
                                      std::vector<std::string> const& args,
                                      TableAsyncCommandType const& command) {
  auto adapter = [=](std::vector<std::string> argv) {
    std::vector<std::string> const common{"<project-id>", "<instance-id>",
                                          "<table-id>"};
    if ((argv.size() == 1 && argv[0] == "--help") ||
        argv.size() != common.size() + args.size()) {
      std::ostringstream os;
      os << name;
      for (auto const& a : common) {
        os << " " << a;
      }
      for (auto const& a : args) {
        os << " " << a;
      }
      throw Usage{std::move(os).str()};
    }
    google::cloud::bigtable::Table table(
        google::cloud::bigtable::CreateDefaultDataClient(
            argv[0], argv[1], google::cloud::bigtable::ClientOptions()),
        argv[2]);
    google::cloud::CompletionQueue cq;
    std::thread t([&cq] { cq.Run(); });
    AutoShutdownCQ shutdown(cq, std::move(t));
    argv.erase(argv.begin(), argv.begin() + common.size());
    command(std::move(table), cq, std::move(argv));
  };
  return Commands::value_type{name, std::move(adapter)};
}

Commands::value_type MakeCommandEntry(
    std::string const& name, std::vector<std::string> const& args,
    InstanceAdminAsyncCommandType const& command) {
  auto adapter = [=](std::vector<std::string> argv) {
    std::vector<std::string> const common{"<project-id>"};
    if ((argv.size() == 1 && argv[0] == "--help") ||
        argv.size() != common.size() + args.size()) {
      std::ostringstream os;
      os << name;
      for (auto const& a : common) {
        os << " " << a;
      }
      for (auto const& a : args) {
        os << " " << a;
      }
      throw Usage{std::move(os).str()};
    }
    google::cloud::bigtable::InstanceAdmin admin(
        google::cloud::bigtable::CreateDefaultInstanceAdminClient(
            argv[0], google::cloud::bigtable::ClientOptions()));
    google::cloud::CompletionQueue cq;
    std::thread t([&cq] { cq.Run(); });
    AutoShutdownCQ shutdown(cq, std::move(t));
    argv.erase(argv.begin(), argv.begin() + common.size());
    command(std::move(admin), cq, std::move(argv));
  };
  return Commands::value_type{name, std::move(adapter)};
}

Commands::value_type MakeCommandEntry(
    std::string const& name, std::vector<std::string> const& args,
    TableAdminAsyncCommandType const& command) {
  auto adapter = [=](std::vector<std::string> argv) {
    std::vector<std::string> const common{"<project-id>", "<instance-id>"};
    if ((argv.size() == 1 && argv[0] == "--help") ||
        argv.size() != common.size() + args.size()) {
      std::ostringstream os;
      os << name;
      for (auto const& a : common) {
        os << " " << a;
      }
      for (auto const& a : args) {
        os << " " << a;
      }
      throw Usage{std::move(os).str()};
    }
    google::cloud::bigtable::TableAdmin admin(
        google::cloud::bigtable::CreateDefaultAdminClient(
            argv[0], google::cloud::bigtable::ClientOptions()),
        argv[1]);
    google::cloud::CompletionQueue cq;
    std::thread t([&cq] { cq.Run(); });
    AutoShutdownCQ shutdown(cq, std::move(t));
    argv.erase(argv.begin(), argv.begin() + common.size());
    command(std::move(admin), cq, std::move(argv));
  };
  return Commands::value_type{name, std::move(adapter)};
}

}  // namespace examples
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
