// Copyright 2018 Google LLC
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

#include "google/cloud/storage/lifecycle_rule.h"
#include "google/cloud/storage/internal/bucket_requests.h"
#include "google/cloud/storage/storage_class.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
LifecycleRule CreateLifecycleRuleForTest() {
  std::string text = R"""({
      "condition": {
        "age": 42,
        "createdBefore": "2018-07-23T12:00:00Z",
        "isLive": true,
        "matchesStorageClass": [ "STANDARD" ],
        "numNewerVersions": 7
      },
      "action": {
        "type": "SetStorageClass",
        "storageClass": "NEARLINE"
      }
    })""";
  return internal::LifecycleRuleParser::FromString(text).value();
}

/// @test Verify that LifecycleRuleAction streaming works as expected.
TEST(LifecycleRuleTest, LifecycleRuleActionStream) {
  LifecycleRuleAction action = LifecycleRule::SetStorageClassStandard();
  std::ostringstream os;
  os << action;
  auto actual = os.str();
  EXPECT_THAT(actual, ::testing::HasSubstr("SetStorageClass"));
  EXPECT_THAT(actual, ::testing::HasSubstr("STANDARD"));
}

/// @test Verify that LifecycleRule::DeleteAction() works as expected.
TEST(LifecycleRuleTest, DeleteAction) {
  EXPECT_EQ("Delete", LifecycleRule::Delete().type);
}

/// @test Verify that LifecycleRule::SetStorageClass*() work as expected.
TEST(LifecycleRuleTest, SetStorageClass) {
  EXPECT_EQ("SetStorageClass", LifecycleRule::SetStorageClass("foo").type);
  EXPECT_EQ("foo", LifecycleRule::SetStorageClass("foo").storage_class);
  EXPECT_EQ(LifecycleRule::SetStorageClass(storage_class::Standard()),
            LifecycleRule::SetStorageClassStandard());

  EXPECT_EQ(LifecycleRule::SetStorageClass(storage_class::MultiRegional()),
            LifecycleRule::SetStorageClassMultiRegional());
  EXPECT_EQ(LifecycleRule::SetStorageClass(storage_class::Regional()),
            LifecycleRule::SetStorageClassRegional());
  EXPECT_EQ(LifecycleRule::SetStorageClass(storage_class::Nearline()),
            LifecycleRule::SetStorageClassNearline());
  EXPECT_EQ(LifecycleRule::SetStorageClass(storage_class::Coldline()),
            LifecycleRule::SetStorageClassColdline());
  EXPECT_EQ(LifecycleRule::SetStorageClass(
                storage_class::DurableReducedAvailability()),
            LifecycleRule::SetStorageClassDurableReducedAvailability());
  EXPECT_EQ(LifecycleRule::SetStorageClass(storage_class::Archive()),
            LifecycleRule::SetStorageClassArchive());
}

/// @test Verify that LifecycleRuleCondition comparisons work as expected.
TEST(LifecycleRuleTest, ConditionCompare) {
  EXPECT_EQ(LifecycleRule::MaxAge(42), LifecycleRule::MaxAge(42));
  EXPECT_NE(LifecycleRule::MaxAge(42), LifecycleRule::MaxAge(7));
  EXPECT_NE(LifecycleRule::MaxAge(42),
            LifecycleRule::CreatedBefore("2018-07-23T12:00:00Z"));
  EXPECT_NE(LifecycleRule::MaxAge(42), LifecycleRule::IsLive(true));
  EXPECT_NE(LifecycleRule::MaxAge(42),
            LifecycleRule::MatchesStorageClassStandard());
  EXPECT_NE(LifecycleRule::MaxAge(42), LifecycleRule::NumNewerVersions(1));
}

/// @test Verify that the LifecycleRuleCondition streaming operator works as
/// expected.
TEST(LifecycleRuleTest, ConditionStream) {
  auto c1 = LifecycleRule::NumNewerVersions(7);
  auto c2 = LifecycleRule::MaxAge(42);
  auto c3 = LifecycleRule::MatchesStorageClasses({storage_class::Nearline(),
                                                  storage_class::Standard(),
                                                  storage_class::Regional()});
  auto condition = LifecycleRule::ConditionConjunction(c1, c2, c3);
  std::ostringstream os;
  os << condition;
  auto actual = os.str();
  EXPECT_THAT(actual, ::testing::HasSubstr("age=42"));
  EXPECT_THAT(actual, ::testing::HasSubstr("num_newer_versions=7"));
  EXPECT_THAT(actual,
              ::testing::HasSubstr(
                  "matches_storage_class=[NEARLINE, STANDARD, REGIONAL]"));
  EXPECT_THAT(actual, ::testing::Not(::testing::HasSubstr("created_before")));
  EXPECT_THAT(actual, ::testing::Not(::testing::HasSubstr("is_live")));
}

/// @test Verify that LifecycleRule::MaxAge() works as expected.
TEST(LifecycleRuleTest, MaxAge) {
  auto condition = LifecycleRule::MaxAge(42);
  ASSERT_TRUE(condition.age.has_value());
  EXPECT_EQ(42, condition.age.value());
}

/// @test Verify that LifecycleRule::CreatedBefore(string) works as expected.
TEST(LifecycleRuleTest, CreatedBeforeString) {
  auto condition = LifecycleRule::CreatedBefore("2018-07-01T12:00:00Z");
  ASSERT_TRUE(condition.created_before.has_value());
  auto expected = google::cloud::internal::ParseRfc3339("2018-07-01T12:00:00Z");
  EXPECT_EQ(expected, condition.created_before.value());
}

/// @test Verify that LifecycleRule::CreatedBefore(time_point) works as
/// expected.
TEST(LifecycleRuleTest, CreatedBeforeTimePoint) {
  auto expected = google::cloud::internal::ParseRfc3339("2018-07-01T12:00:00Z");
  auto condition = LifecycleRule::CreatedBefore(expected);
  ASSERT_TRUE(condition.created_before.has_value());
  EXPECT_EQ(expected, condition.created_before.value());
}

/// @test Verify that LifecycleRule::IsLive works as expected.
TEST(LifecycleRuleTest, IsLiveTrue) {
  auto condition = LifecycleRule::IsLive(true);
  ASSERT_TRUE(condition.is_live.has_value());
  EXPECT_TRUE(condition.is_live.value());
}

/// @test Verify that LifecycleRule::IsLive works as expected.
TEST(LifecycleRuleTest, IsLiveFalse) {
  auto condition = LifecycleRule::IsLive(false);
  ASSERT_TRUE(condition.is_live.has_value());
  EXPECT_FALSE(condition.is_live.value());
}

/// @test Verify that LifecycleRule::MatchesStorageClass works as expected.
TEST(LifecycleRuleTest, MatchesStorageClass) {
  auto condition = LifecycleRule::MatchesStorageClass("foo");
  ASSERT_TRUE(condition.matches_storage_class.has_value());
  ASSERT_FALSE(condition.matches_storage_class->empty());
  EXPECT_EQ("foo", condition.matches_storage_class->front());
}

/// @test Verify that LifecycleRule::MatchesStorageClasses works as expected.
TEST(LifecycleRuleTest, MatchesStorageClasses) {
  auto condition = LifecycleRule::MatchesStorageClasses(
      {storage_class::Standard(), storage_class::Regional()});
  ASSERT_TRUE(condition.matches_storage_class.has_value());
  EXPECT_THAT(*condition.matches_storage_class,
              ::testing::ElementsAre(storage_class::Standard(),
                                     storage_class::Regional()));
}

/// @test Verify that LifecycleRule::MatchesStorageClasses works as expected.
TEST(LifecycleRuleTest, MatchesStorageClassesIterator) {
  std::set<std::string> classes{storage_class::Standard(),
                                storage_class::Regional()};
  auto condition =
      LifecycleRule::MatchesStorageClasses(classes.begin(), classes.end());
  ASSERT_TRUE(condition.matches_storage_class.has_value());
  EXPECT_THAT(*condition.matches_storage_class,
              ::testing::UnorderedElementsAre(storage_class::Standard(),
                                              storage_class::Regional()));
}

/// @test LifecycleRule::MatchesStorageClassStandard.
TEST(LifecycleRuleTest, MatchesStorageClassStandard) {
  auto condition = LifecycleRule::MatchesStorageClassStandard();
  ASSERT_TRUE(condition.matches_storage_class.has_value());
  ASSERT_FALSE(condition.matches_storage_class->empty());
  EXPECT_EQ(storage_class::Standard(),
            condition.matches_storage_class->front());
}

/// @test LifecycleRule::MatchesStorageClassMultiRegional.
TEST(LifecycleRuleTest, MatchesStorageClassMultiRegional) {
  auto condition = LifecycleRule::MatchesStorageClassMultiRegional();
  ASSERT_TRUE(condition.matches_storage_class.has_value());
  ASSERT_FALSE(condition.matches_storage_class->empty());
  EXPECT_EQ(storage_class::MultiRegional(),
            condition.matches_storage_class->front());
}

/// @test LifecycleRule::MatchesStorageClassRegional.
TEST(LifecycleRuleTest, MatchesStorageClassRegional) {
  auto condition = LifecycleRule::MatchesStorageClassRegional();
  ASSERT_TRUE(condition.matches_storage_class.has_value());
  ASSERT_FALSE(condition.matches_storage_class->empty());
  EXPECT_EQ(storage_class::Regional(),
            condition.matches_storage_class->front());
}

/// @test LifecycleRule::MatchesStorageClassNearline.
TEST(LifecycleRuleTest, MatchesStorageClassNearline) {
  auto condition = LifecycleRule::MatchesStorageClassNearline();
  ASSERT_TRUE(condition.matches_storage_class.has_value());
  ASSERT_FALSE(condition.matches_storage_class->empty());
  EXPECT_EQ(storage_class::Nearline(),
            condition.matches_storage_class->front());
}

/// @test LifecycleRule::MatchesStorageClassColdline.
TEST(LifecycleRuleTest, MatchesStorageClassColdline) {
  auto condition = LifecycleRule::MatchesStorageClassColdline();
  ASSERT_TRUE(condition.matches_storage_class.has_value());
  ASSERT_FALSE(condition.matches_storage_class->empty());
  EXPECT_EQ(storage_class::Coldline(),
            condition.matches_storage_class->front());
}

/// @test LifecycleRule::MatchesStorageClassDurableReducedAvailability.
TEST(LifecycleRuleTest, MatchesStorageClassDurableReducedAvailability) {
  auto condition =
      LifecycleRule::MatchesStorageClassDurableReducedAvailability();
  ASSERT_TRUE(condition.matches_storage_class.has_value());
  ASSERT_FALSE(condition.matches_storage_class->empty());
  EXPECT_EQ(storage_class::DurableReducedAvailability(),
            condition.matches_storage_class->front());
}

/// @test LifecycleRule::MatchesStorageClassArchive.
TEST(LifecycleRuleTest, MatchesStorageClassArchive) {
  auto condition = LifecycleRule::MatchesStorageClassArchive();
  ASSERT_TRUE(condition.matches_storage_class.has_value());
  ASSERT_FALSE(condition.matches_storage_class->empty());
  EXPECT_EQ(storage_class::Archive(), condition.matches_storage_class->front());
}

/// @test Verify that LifecycleRule::NumNewerVersions() works as expected.
TEST(LifecycleRuleTest, NumNewerVersions) {
  auto condition = LifecycleRule::NumNewerVersions(7);
  ASSERT_TRUE(condition.num_newer_versions.has_value());
  EXPECT_EQ(7, condition.num_newer_versions.value());
}

/// @test Verify that LifecycleRule::ConditionConjunction() works as expected.
TEST(LifecycleRuleTest, ConditionConjunctionAge) {
  auto c1 = LifecycleRule::MaxAge(7);
  auto c2 = LifecycleRule::MaxAge(42);
  auto condition = LifecycleRule::ConditionConjunction(c1, c2);
  ASSERT_TRUE(condition.age.has_value());
  EXPECT_EQ(7, condition.age.value());
}

/// @test Verify that LifecycleRule::ConditionConjunction() works as expected.
TEST(LifecycleRuleTest, ConditionConjunctionCreatedBefore) {
  auto c1 = LifecycleRule::CreatedBefore("2018-01-08T12:00:00Z");
  auto c2 = LifecycleRule::CreatedBefore("2018-02-08T12:00:00Z");
  auto condition = LifecycleRule::ConditionConjunction(c1, c2);
  ASSERT_TRUE(condition.created_before.has_value());
  EXPECT_EQ(c2.created_before.value(), condition.created_before.value());
}

/// @test Verify that LifecycleRule::ConditionConjunction() works as expected.
TEST(LifecycleRuleTest, ConditionConjunctionIsLiveInvalid) {
  auto c1 = LifecycleRule::IsLive(true);
  auto c2 = LifecycleRule::IsLive(false);
#ifdef GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try {
        LifecycleRule::ConditionConjunction(c1, c2);
      } catch (std::exception const& ex) {
        EXPECT_THAT(ex.what(), ::testing::HasSubstr("LifecycleRule"));
        throw;
      },
      std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(LifecycleRule::ConditionConjunction(c1, c2), "");
#endif
}

/// @test Verify that LifecycleRule::ConditionConjunction() works as expected.
TEST(LifecycleRuleTest, ConditionConjunctionIsLiveTrue) {
  auto c1 = LifecycleRule::IsLive(true);
  auto c2 = LifecycleRule::IsLive(true);
  auto condition = LifecycleRule::ConditionConjunction(c1, c2);
  ASSERT_TRUE(condition.is_live.has_value());
  EXPECT_TRUE(condition.is_live.value());
}

/// @test Verify that LifecycleRule::ConditionConjunction() works as expected.
TEST(LifecycleRuleTest, ConditionConjunctionIsLiveFalse) {
  auto c1 = LifecycleRule::IsLive(true);
  auto c2 = LifecycleRule::IsLive(true);
  auto condition = LifecycleRule::ConditionConjunction(c1, c2);
  ASSERT_TRUE(condition.is_live.has_value());
  EXPECT_TRUE(condition.is_live.value());
}

/// @test Verify that LifecycleRule::ConditionConjunction() works as expected.
TEST(LifecycleRuleTest, ConditionConjunctionMatchesStorageClass) {
  auto c1 = LifecycleRule::MatchesStorageClasses({storage_class::Nearline(),
                                                  storage_class::Standard(),
                                                  storage_class::Coldline()});
  auto c2 = LifecycleRule::MatchesStorageClasses({storage_class::Nearline(),
                                                  storage_class::Standard(),
                                                  storage_class::Regional()});
  auto condition = LifecycleRule::ConditionConjunction(c1, c2);
  ASSERT_TRUE(condition.matches_storage_class.has_value());
  EXPECT_THAT(*condition.matches_storage_class,
              ::testing::UnorderedElementsAre(storage_class::Standard(),
                                              storage_class::Nearline()));
}

/// @test Verify that LifecycleRule::ConditionConjunction() works as expected.
TEST(LifecycleRuleTest, ConditionConjunctionNumNewerVersions) {
  auto c1 = LifecycleRule::NumNewerVersions(7);
  auto c2 = LifecycleRule::NumNewerVersions(42);
  auto condition = LifecycleRule::ConditionConjunction(c1, c2);
  ASSERT_TRUE(condition.num_newer_versions.has_value());
  EXPECT_EQ(42, *condition.num_newer_versions);
}

/// @test Verify that LifecycleRule::ConditionConjunction() works as expected.
TEST(LifecycleRuleTest, ConditionConjunctionMultiple) {
  auto c1 = LifecycleRule::NumNewerVersions(7);
  auto c2 = LifecycleRule::MaxAge(42);
  auto c3 = LifecycleRule::MatchesStorageClasses({storage_class::Nearline(),
                                                  storage_class::Standard(),
                                                  storage_class::Regional()});
  auto condition = LifecycleRule::ConditionConjunction(c1, c2, c3);
  ASSERT_TRUE(condition.age.has_value());
  EXPECT_EQ(42, *condition.age);
  EXPECT_FALSE(condition.created_before.has_value());
  EXPECT_FALSE(condition.is_live.has_value());
  ASSERT_TRUE(condition.num_newer_versions.has_value());
  EXPECT_EQ(7, *condition.num_newer_versions);
  ASSERT_TRUE(condition.matches_storage_class.has_value());
  EXPECT_THAT(*condition.matches_storage_class,
              ::testing::UnorderedElementsAre(storage_class::Nearline(),
                                              storage_class::Standard(),
                                              storage_class::Regional()));
}

/// @test Verify that LifecycleRule parsing works as expected.
TEST(LifecycleRuleTest, Parsing) {
  // This function uses ParseFromString() to create the LifecycleRule.
  LifecycleRule actual = CreateLifecycleRuleForTest();
  LifecycleRuleCondition expected_condition =
      LifecycleRule::ConditionConjunction(
          LifecycleRule::MaxAge(42),
          LifecycleRule::CreatedBefore("2018-07-23T12:00:00Z"),
          LifecycleRule::IsLive(true),
          LifecycleRule::MatchesStorageClassStandard(),
          LifecycleRule::NumNewerVersions(7));
  EXPECT_EQ(expected_condition, actual.condition());

  LifecycleRuleAction expected_action =
      LifecycleRule::SetStorageClassNearline();
  EXPECT_EQ(expected_action, actual.action());
}

/// @test Verify that LifecycleRule streaming operator works as expected.
TEST(LifecycleRuleTest, LifecycleRuleStream) {
  auto rule = CreateLifecycleRuleForTest();
  std::ostringstream os;
  os << rule;
  auto actual = os.str();
  EXPECT_THAT(actual, ::testing::HasSubstr("age=42"));
  EXPECT_THAT(actual, ::testing::HasSubstr("NEARLINE"));
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
