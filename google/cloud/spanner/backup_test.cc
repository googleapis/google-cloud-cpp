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

#include "google/cloud/spanner/backup.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <sstream>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Eq;

TEST(Backup, Basics) {
  Instance in("p1", "i1");
  Backup backup(in, "b1");
  EXPECT_EQ("b1", backup.backup_id());
  EXPECT_EQ(in, backup.instance());
  EXPECT_EQ("projects/p1/instances/i1/backups/b1", backup.FullName());

  auto copy = backup;
  EXPECT_EQ(copy, backup);
  EXPECT_EQ("b1", copy.backup_id());
  EXPECT_EQ(in, copy.instance());
  EXPECT_EQ("projects/p1/instances/i1/backups/b1", copy.FullName());

  auto moved = std::move(copy);
  EXPECT_EQ(moved, backup);
  EXPECT_EQ("b1", moved.backup_id());
  EXPECT_EQ(in, moved.instance());
  EXPECT_EQ("projects/p1/instances/i1/backups/b1", moved.FullName());

  Instance in2("p2", "i2");
  Backup backup2(in2, "b2");
  EXPECT_NE(backup2, backup);
  EXPECT_EQ("b2", backup2.backup_id());
  EXPECT_EQ(in2, backup2.instance());
  EXPECT_EQ("projects/p2/instances/i2/backups/b2", backup2.FullName());
}

TEST(Backup, OutputStream) {
  Instance in("p1", "i1");
  Backup backup(in, "b1");
  std::ostringstream os;
  os << backup;
  EXPECT_EQ("projects/p1/instances/i1/backups/b1", os.str());
}

TEST(Backup, MakeBackup) {
  auto constexpr kValidBackupName = "projects/p1/instances/i1/backups/b1";
  auto bu = MakeBackup(kValidBackupName);
  ASSERT_THAT(bu, IsOk());
  EXPECT_THAT(bu->FullName(), Eq(kValidBackupName));

  for (std::string invalid : {
           "",
           "projects/",
           "projects/p1",
           "projects/p1/instances/",
           "projects/p1/instances/i1",
           "projects/p1/instances/i1/backups",
           "projects/p1/instances/i1/backups/",
           "/projects/p1/instances/i1/backups/b1",
           "projects/p1/instances/i1/backups/b1/",
           "projects/p1/instances/i1/backups/b1/etc",
       }) {
    auto bu = MakeBackup(invalid);
    EXPECT_THAT(bu, StatusIs(StatusCode::kInvalidArgument,
                             "Improperly formatted Backup: " + invalid));
  }
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
