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

#include "google/cloud/bigtable/examples/bigtable_examples_common.h"
#include "google/cloud/bigtable/testing/mock_admin_client.h"
#include "google/cloud/bigtable/testing/mock_instance_admin_client.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include <gmock/gmock.h>
#include <stdexcept>

namespace google {
namespace cloud {
namespace bigtable {
namespace examples {

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::ReturnRef;
using ::testing::StartsWith;

TEST(BigtableExamplesCommon, RunAdminIntegrationTestsEmulator) {
  google::cloud::testing_util::ScopedEnvironment emulator(
      "BIGTABLE_EMULATOR_HOST", "localhost:9090");
  google::cloud::testing_util::ScopedEnvironment admin(
      "ENABLE_BIGTABLE_ADMIN_INTEGRATION_TESTS", "no");
  EXPECT_TRUE(RunAdminIntegrationTests());
}

TEST(BigtableExamplesCommon, RunAdminIntegrationTestsProductionAndDisabled) {
  google::cloud::testing_util::ScopedEnvironment emulator(
      "BIGTABLE_EMULATOR_HOST", {});
  google::cloud::testing_util::ScopedEnvironment admin(
      "ENABLE_BIGTABLE_ADMIN_INTEGRATION_TESTS", "no");
  EXPECT_FALSE(RunAdminIntegrationTests());
}

TEST(BigtableExamplesCommon, RunAdminIntegrationTestsProductionAndEnabled) {
  google::cloud::testing_util::ScopedEnvironment emulator(
      "BIGTABLE_EMULATOR_HOST", {});
  google::cloud::testing_util::ScopedEnvironment admin(
      "ENABLE_BIGTABLE_ADMIN_INTEGRATION_TESTS", "yes");
  EXPECT_TRUE(RunAdminIntegrationTests());
}

TEST(BigtableExamplesCommon, MakeTableAdminCommandEntry) {
  // Pretend we are using the emulator to avoid loading the default
  // credentials from $HOME, which do not exist when running with Bazel.
  google::cloud::testing_util::ScopedEnvironment emulator(
      "BIGTABLE_EMULATOR_HOST", "localhost:9090");
  int call_count = 0;
  auto command = [&call_count](bigtable::TableAdmin const&,
                               std::vector<std::string> const& argv) {
    ++call_count;
    ASSERT_EQ(2, argv.size());
    EXPECT_EQ("a", argv[0]);
    EXPECT_EQ("b", argv[1]);
  };
  auto const actual = MakeCommandEntry("command-name", {"foo", "bar"}, command);
  EXPECT_EQ("command-name", actual.first);
  EXPECT_THROW(
      try { actual.second({}); } catch (Usage const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("command-name"));
        EXPECT_THAT(ex.what(), HasSubstr("foo"));
        EXPECT_THAT(ex.what(), HasSubstr("bar"));
        throw;
      },
      Usage);

  ASSERT_NO_FATAL_FAILURE(actual.second({"unused", "unused", "a", "b"}));
  EXPECT_EQ(1, call_count);
}

TEST(BigtableExamplesCommon, CleanupOldTables) {
  using MockAdminClient = ::google::cloud::bigtable::testing::MockAdminClient;
  namespace btadmin = google::bigtable::admin::v2;

  auto const expired_tp =
      std::chrono::system_clock::now() - std::chrono::hours(72);
  auto const active_tp = std::chrono::system_clock::now();
  auto const id_1 = TablePrefix("test-", expired_tp) + "0001";
  auto const id_2 = TablePrefix("test-", expired_tp) + "0002";
  auto const id_3 = TablePrefix("test-", active_tp) + "0003";
  auto const id_4 = TablePrefix("test-", active_tp) + "0004";
  auto const id_5 = TablePrefix("exclude-", expired_tp) + "0005";

  std::string const project_id = "test-project-id";
  std::string const instance_id = "test-instance-id";

  auto mock = std::make_shared<MockAdminClient>();
  EXPECT_CALL(*mock, project()).WillRepeatedly(ReturnRef(project_id));

  EXPECT_CALL(*mock, ListTables(_, _, _))
      .WillOnce(Invoke([&](grpc::ClientContext*,
                           btadmin::ListTablesRequest const& request,
                           btadmin::ListTablesResponse* response) {
        for (auto const& id : {id_1, id_2, id_3, id_4, id_5}) {
          auto& instance = *response->add_tables();
          instance.set_name(request.parent() + "/tables/" + id);
        }
        response->clear_next_page_token();
        return grpc::Status::OK;
      }));

  bigtable::TableAdmin admin(mock, instance_id);
  auto const name_1 = admin.TableName(id_1);
  auto const name_2 = admin.TableName(id_2);

  // Verify only `name_1` and `name_2` are deleted.
  EXPECT_CALL(*mock, DeleteTable(_, _, _))
      .WillOnce(Invoke([&](grpc::ClientContext*,
                           btadmin::DeleteTableRequest const& request,
                           google::protobuf::Empty*) {
        EXPECT_EQ(request.name(), name_1);
        return grpc::Status::OK;
      }))
      .WillOnce(Invoke([&](grpc::ClientContext*,
                           btadmin::DeleteTableRequest const& request,
                           google::protobuf::Empty*) {
        EXPECT_EQ(request.name(), name_2);
        return grpc::Status::OK;
      }));

  CleanupOldTables("test-", admin);
}

TEST(BigtableExamplesCommon, RandomInstanceId) {
  auto generator = google::cloud::internal::DefaultPRNG();
  auto const id_1 = RandomInstanceId("test-", generator);
  auto const id_2 = RandomInstanceId("test-", generator);
  EXPECT_THAT(id_1, StartsWith("test-"));
  EXPECT_THAT(id_2, StartsWith("test-"));
  EXPECT_NE(id_1, id_2);
}

TEST(BigtableExamplesCommon, RandomInstanceIdTooLong) {
  auto generator = google::cloud::internal::DefaultPRNG();
  EXPECT_THROW(RandomInstanceId("this-prefix-is-too-long-by-half", generator),
               std::invalid_argument);
}

TEST(BigtableExamplesCommon, RandomClusterId) {
  auto generator = google::cloud::internal::DefaultPRNG();
  auto const id_1 = RandomClusterId("test-", generator);
  auto const id_2 = RandomClusterId("test-", generator);
  EXPECT_THAT(id_1, StartsWith("test-"));
  EXPECT_THAT(id_2, StartsWith("test-"));
  EXPECT_NE(id_1, id_2);
}

TEST(BigtableExamplesCommon, RandomClusterIdTooLong) {
  auto generator = google::cloud::internal::DefaultPRNG();
  EXPECT_THROW(RandomClusterId("this-prefix-is-too-long-by-half", generator),
               std::invalid_argument);
}

TEST(BigtableExamplesCommon, CleanupOldInstances) {
  using MockAdminClient =
      ::google::cloud::bigtable::testing::MockInstanceAdminClient;
  namespace btadmin = google::bigtable::admin::v2;

  auto const expired_tp =
      std::chrono::system_clock::now() - std::chrono::hours(72);
  auto const active_tp = std::chrono::system_clock::now();
  auto const id_1 = InstancePrefix("test-", expired_tp) + "0001";
  auto const id_2 = InstancePrefix("test-", expired_tp) + "0002";
  auto const id_3 = InstancePrefix("test-", active_tp) + "0003";
  auto const id_4 = InstancePrefix("test-", active_tp) + "0004";
  auto const id_5 = InstancePrefix("exclude-", expired_tp) + "0005";

  std::string const project_id = "test-project-id";

  auto mock = std::make_shared<MockAdminClient>();
  EXPECT_CALL(*mock, project()).WillRepeatedly(ReturnRef(project_id));

  EXPECT_CALL(*mock, ListInstances(_, _, _))
      .WillOnce(Invoke([&](grpc::ClientContext*,
                           btadmin::ListInstancesRequest const& request,
                           btadmin::ListInstancesResponse* response) {
        for (auto const& id : {id_1, id_2, id_3, id_4, id_5}) {
          auto& instance = *response->add_instances();
          instance.set_name(request.parent() + "/instances/" + id);
        }
        response->clear_next_page_token();
        return grpc::Status::OK;
      }));

  bigtable::InstanceAdmin admin(mock);
  auto const name_1 = admin.InstanceName(id_1);
  auto const name_2 = admin.InstanceName(id_2);

  // Verify only `name_1` and `name_2` are deleted.
  EXPECT_CALL(*mock, DeleteInstance(_, _, _))
      .WillOnce(Invoke([&](grpc::ClientContext*,
                           btadmin::DeleteInstanceRequest const& request,
                           google::protobuf::Empty*) {
        EXPECT_EQ(request.name(), name_1);
        return grpc::Status::OK;
      }))
      .WillOnce(Invoke([&](grpc::ClientContext*,
                           btadmin::DeleteInstanceRequest const& request,
                           google::protobuf::Empty*) {
        EXPECT_EQ(request.name(), name_2);
        return grpc::Status::OK;
      }));

  CleanupOldInstances("test-", admin);
}

TEST(BigtableExamplesCommon, MakeInstanceAdminCommandEntry) {
  // Pretend we are using the emulator to avoid loading the default
  // credentials from $HOME, which do not exist when running with Bazel.
  google::cloud::testing_util::ScopedEnvironment emulator(
      "BIGTABLE_EMULATOR_HOST", "localhost:9090");

  int call_count = 0;
  auto command = [&call_count](bigtable::InstanceAdmin const&,
                               std::vector<std::string> const& argv) {
    ++call_count;
    ASSERT_EQ(2, argv.size());
    EXPECT_EQ("a", argv[0]);
    EXPECT_EQ("b", argv[1]);
  };
  auto const actual = MakeCommandEntry("command-name", {"foo", "bar"}, command);
  EXPECT_EQ("command-name", actual.first);
  EXPECT_THROW(
      try { actual.second({}); } catch (Usage const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("command-name"));
        EXPECT_THAT(ex.what(), HasSubstr("foo"));
        EXPECT_THAT(ex.what(), HasSubstr("bar"));
        throw;
      },
      Usage);

  ASSERT_NO_FATAL_FAILURE(actual.second({"unused", "a", "b"}));
  EXPECT_EQ(1, call_count);
}

TEST(BigtableExamplesCommon, MakeTableAsyncCommandEntry) {
  // Pretend we are using the emulator to avoid loading the default
  // credentials from $HOME, which do not exist when running with Bazel.
  google::cloud::testing_util::ScopedEnvironment emulator(
      "BIGTABLE_EMULATOR_HOST", "localhost:9090");

  int call_count = 0;
  auto command = [&call_count](bigtable::Table const&, CompletionQueue const&,
                               std::vector<std::string> const& argv) {
    ++call_count;
    ASSERT_EQ(2, argv.size());
    EXPECT_EQ("a", argv[0]);
    EXPECT_EQ("b", argv[1]);
  };
  auto const actual = MakeCommandEntry("command-name", {"foo", "bar"}, command);
  EXPECT_EQ("command-name", actual.first);
  EXPECT_THROW(
      try { actual.second({}); } catch (Usage const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("command-name"));
        EXPECT_THAT(ex.what(), HasSubstr("foo"));
        EXPECT_THAT(ex.what(), HasSubstr("bar"));
        throw;
      },
      Usage);

  ASSERT_NO_FATAL_FAILURE(actual.second(
      {"unused-project", "unused-instance", "unused-table", "a", "b"}));
  EXPECT_EQ(1, call_count);
}

TEST(BigtableExamplesCommon, MakeInstanceAdminAsyncCommandEntry) {
  // Pretend we are using the emulator to avoid loading the default
  // credentials from $HOME, which do not exist when running with Bazel.
  google::cloud::testing_util::ScopedEnvironment emulator(
      "BIGTABLE_EMULATOR_HOST", "localhost:9090");

  int call_count = 0;
  auto command = [&call_count](bigtable::InstanceAdmin const&,
                               CompletionQueue const&,
                               std::vector<std::string> const& argv) {
    ++call_count;
    ASSERT_EQ(2, argv.size());
    EXPECT_EQ("a", argv[0]);
    EXPECT_EQ("b", argv[1]);
  };
  auto const actual = MakeCommandEntry("command-name", {"foo", "bar"}, command);
  EXPECT_EQ("command-name", actual.first);
  EXPECT_THROW(
      try { actual.second({}); } catch (Usage const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("command-name"));
        EXPECT_THAT(ex.what(), HasSubstr("foo"));
        EXPECT_THAT(ex.what(), HasSubstr("bar"));
        throw;
      },
      Usage);

  ASSERT_NO_FATAL_FAILURE(actual.second({"unused-project", "a", "b"}));
  EXPECT_EQ(1, call_count);
}

TEST(BigtableExamplesCommon, MakeTableAdminAsyncCommandEntry) {
  // Pretend we are using the emulator to avoid loading the default
  // credentials from $HOME, which do not exist when running with Bazel.
  google::cloud::testing_util::ScopedEnvironment emulator(
      "BIGTABLE_EMULATOR_HOST", "localhost:9090");

  int call_count = 0;
  auto command = [&call_count](bigtable::TableAdmin const&,
                               CompletionQueue const&,
                               std::vector<std::string> const& argv) {
    ++call_count;
    ASSERT_EQ(2, argv.size());
    EXPECT_EQ("a", argv[0]);
    EXPECT_EQ("b", argv[1]);
  };
  auto const actual = MakeCommandEntry("command-name", {"foo", "bar"}, command);
  EXPECT_EQ("command-name", actual.first);
  EXPECT_THROW(
      try { actual.second({}); } catch (Usage const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("command-name"));
        EXPECT_THAT(ex.what(), HasSubstr("foo"));
        EXPECT_THAT(ex.what(), HasSubstr("bar"));
        throw;
      },
      Usage);

  ASSERT_NO_FATAL_FAILURE(
      actual.second({"unused-project", "unused-instance", "a", "b"}));
  EXPECT_EQ(1, call_count);
}

}  // namespace examples
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
