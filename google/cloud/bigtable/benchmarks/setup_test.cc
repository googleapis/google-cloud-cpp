// Copyright 2017 Google Inc.
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

#include "google/cloud/bigtable/benchmarks/setup.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>

using namespace google::cloud::bigtable::benchmarks;

namespace {
char arg0[] = "program";
char arg1[] = "foo";
char arg2[] = "bar";
char arg3[] = "profile";
char arg4[] = "4";
char arg5[] = "300";
char arg6[] = "10000";
char arg7[] = "True";
char arg8[] = "Unused";
}  // anonymous namespace

TEST(BenchmarksSetup, Basic) {
  char* argv[] = {arg0, arg1, arg2, arg3};
  int argc = 4;
  auto setup = MakeBenchmarkSetup("pre", argc, argv);
  ASSERT_STATUS_OK(setup);
  EXPECT_EQ("foo", setup->project_id());
  EXPECT_EQ("bar", setup->instance_id());
  EXPECT_EQ("profile", setup->app_profile_id());
  EXPECT_EQ(0, setup->table_id().find("pre"));
  std::size_t expected = 4 + kTableIdRandomLetters;
  EXPECT_EQ(expected, setup->table_id().size());

  EXPECT_EQ(kDefaultTableSize, setup->table_size());
  EXPECT_EQ(kDefaultTestDuration * 60, setup->test_duration().count());
  EXPECT_FALSE(setup->use_embedded_server());
}

TEST(BenchmarksSetup, Different) {
  char arg0[] = "program";
  char arg1[] = "foo";
  char arg2[] = "bar";
  char arg3[] = "profile";
  char* argv_0[] = {arg0, arg1, arg2, arg3};
  int argc_0 = sizeof(argv_0) / sizeof(argv_0[0]);
  char* argv_1[] = {arg0, arg1, arg2, arg3};
  int argc_1 = sizeof(argv_1) / sizeof(argv_1[0]);
  auto s0 = MakeBenchmarkSetup("pre", argc_0, argv_0);
  ASSERT_STATUS_OK(s0);
  auto s1 = MakeBenchmarkSetup("pre", argc_1, argv_1);
  ASSERT_STATUS_OK(s1);
  // The probability of this test failing is tiny, but if it does, run it again.
  // Sorry for the flakiness, but randomness is hard.
  EXPECT_NE(s0->table_id(), s1->table_id());
}

TEST(BenchmarkSetup, Parse) {
  char* argv[] = {arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8};
  int argc = sizeof(argv) / sizeof(argv[0]);
  auto setup = MakeBenchmarkSetup("pre", argc, argv);
  ASSERT_STATUS_OK(setup);

  EXPECT_EQ(2, argc);
  EXPECT_EQ(std::string("program"), argv[0]);
  EXPECT_EQ(std::string("Unused"), argv[1]);

  EXPECT_EQ("foo", setup->project_id());
  EXPECT_EQ("bar", setup->instance_id());
  EXPECT_EQ("profile", setup->app_profile_id());
  EXPECT_EQ(4, setup->thread_count());
  EXPECT_EQ(300, setup->test_duration().count());
  EXPECT_EQ(10000, setup->table_size());
  EXPECT_TRUE(setup->use_embedded_server());
}

TEST(BenchmarkSetup, Test7) {
  char* argv[] = {arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7};
  int argc = sizeof(argv) / sizeof(argv[0]);
  auto setup = MakeBenchmarkSetup("t6", argc, argv);
  ASSERT_STATUS_OK(setup);
  EXPECT_TRUE(setup->use_embedded_server());
}

TEST(BenchmarkSetup, Test6) {
  char* argv[] = {arg0, arg1, arg2, arg3, arg4, arg5, arg6};
  int argc = sizeof(argv) / sizeof(argv[0]);
  auto setup = MakeBenchmarkSetup("t5", argc, argv);
  ASSERT_STATUS_OK(setup);
  EXPECT_EQ(10000, setup->table_size());
  EXPECT_FALSE(setup->use_embedded_server());
}

TEST(BenchmarkSetup, Test5) {
  char* argv[] = {arg0, arg1, arg2, arg3, arg4, arg5};
  int argc = sizeof(argv) / sizeof(argv[0]);
  auto setup = MakeBenchmarkSetup("t4", argc, argv);
  ASSERT_STATUS_OK(setup);
  EXPECT_EQ(300, setup->test_duration().count());
}

TEST(BenchmarkSetup, Test4) {
  char* argv[] = {arg0, arg1, arg2, arg3, arg4};
  int argc = sizeof(argv) / sizeof(argv[0]);
  auto setup = MakeBenchmarkSetup("t3", argc, argv);
  ASSERT_STATUS_OK(setup);
  EXPECT_EQ(4, setup->thread_count());
}

TEST(BenchmarkSetup, Test3) {
  char* argv[] = {arg0, arg1, arg2, arg3};
  int argc = sizeof(argv) / sizeof(argv[0]);
  auto setup = MakeBenchmarkSetup("t2", argc, argv);
  ASSERT_STATUS_OK(setup);
  EXPECT_EQ("foo", setup->project_id());
  EXPECT_EQ("bar", setup->instance_id());
  EXPECT_EQ("profile", setup->app_profile_id());
  EXPECT_EQ(kDefaultThreads, setup->thread_count());
}

TEST(BenchmarkSetup, Test2) {
  char* argv[] = {arg0, arg1, arg2};
  int argc = sizeof(argv) / sizeof(argv[0]);
  EXPECT_FALSE(MakeBenchmarkSetup("t1", argc, argv));
}

TEST(BenchmarkSetup, Test1) {
  char* argv[] = {arg0, arg1};
  int argc = sizeof(argv) / sizeof(argv[0]);
  EXPECT_FALSE(MakeBenchmarkSetup("t1", argc, argv));
}

TEST(BenchmarkSetup, Test0) {
  char* argv[] = {arg0};
  int argc = sizeof(argv) / sizeof(argv[0]);
  EXPECT_FALSE(MakeBenchmarkSetup("t0", argc, argv));
}

TEST(BenchmarkSetup, TestDuration) {
  char seconds[] = "0";
  char* argv[] = {arg0, arg1, arg2, arg3, arg4, seconds, arg6, arg7, arg8};
  int argc = sizeof(argv) / sizeof(argv[0]);

  // Test duration parameter should be >= 0.
  EXPECT_FALSE(MakeBenchmarkSetup("test-duration", argc, argv));
}

TEST(BenchmarkSetup, TableSize) {
  char table_size[] = "10";
  char* argv[] = {arg0, arg1, arg2, arg3, arg4, arg5, table_size, arg7, arg8};
  int argc = sizeof(argv) / sizeof(argv[0]);

  // TableSize parameter should be >= 100.
  EXPECT_FALSE(MakeBenchmarkSetup("table-size", argc, argv));
}
