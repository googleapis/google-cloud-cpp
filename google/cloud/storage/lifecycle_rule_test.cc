// Copyright 2018 Google LLC
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

#include "google/cloud/storage/lifecycle_rule.h"
#include "google/cloud/storage/internal/bucket_requests.h"
#include "google/cloud/storage/internal/lifecycle_rule_parser.h"
#include "google/cloud/storage/storage_class.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::UnorderedElementsAre;

LifecycleRule CreateLifecycleRuleForTest() {
  std::string text = R"""({
      "condition": {
        "age": 42,
        "createdBefore": "2018-07-23",
        "isLive": true,
        "matchesStorageClass": [ "STANDARD" ],
        "numNewerVersions": 7,
        "daysSinceNoncurrentTime": 3,
        "noncurrentTimeBefore": "2020-07-22",
        "daysSinceCustomTime": 30,
        "customTimeBefore": "2020-07-23",
        "matchesPrefix": [ "foo/", "bar/" ],
        "matchesSuffix": [ ".lz4", ".gz" ]
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
  EXPECT_THAT(actual, HasSubstr("SetStorageClass"));
  EXPECT_THAT(actual, HasSubstr("STANDARD"));
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
            LifecycleRule::CreatedBefore(absl::CivilDay(2018, 7, 23)));
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
  EXPECT_THAT(actual, HasSubstr("age=42"));
  EXPECT_THAT(actual, HasSubstr("num_newer_versions=7"));
  EXPECT_THAT(
      actual,
      HasSubstr("matches_storage_class=[NEARLINE, STANDARD, REGIONAL]"));
  EXPECT_THAT(actual, Not(HasSubstr("created_before")));
  EXPECT_THAT(actual, Not(HasSubstr("is_live")));
}

/// @test Verify that LifecycleRule::MaxAge() works as expected.
TEST(LifecycleRuleTest, MaxAge) {
  auto condition = LifecycleRule::MaxAge(42);
  ASSERT_TRUE(condition.age.has_value());
  EXPECT_EQ(42, condition.age.value());
}

/// @test Verify that LifecycleRule::CreatedBefore(Date) works as expected.
TEST(LifecycleRuleTest, CreatedBeforeTimePoint) {
  auto const expected = absl::CivilDay(2020, 7, 26);
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
  EXPECT_THAT(
      *condition.matches_storage_class,
      ElementsAre(storage_class::Standard(), storage_class::Regional()));
}

/// @test Verify that LifecycleRule::MatchesStorageClasses works as expected.
TEST(LifecycleRuleTest, MatchesStorageClassesIterator) {
  std::set<std::string> classes{storage_class::Standard(),
                                storage_class::Regional()};
  auto condition =
      LifecycleRule::MatchesStorageClasses(classes.begin(), classes.end());
  ASSERT_TRUE(condition.matches_storage_class.has_value());
  EXPECT_THAT(*condition.matches_storage_class,
              UnorderedElementsAre(storage_class::Standard(),
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

/// @test Verify that LifecycleRule::DaysSinceNoncurrent() works as expected.
TEST(LifecycleRuleTest, DaysSinceNoncurrentTime) {
  auto const c1 = LifecycleRule::DaysSinceNoncurrentTime(3);
  ASSERT_TRUE(c1.days_since_noncurrent_time.has_value());
  EXPECT_EQ(3, c1.days_since_noncurrent_time.value());
  EXPECT_EQ(c1, c1);
  auto const c2 = LifecycleRule::DaysSinceNoncurrentTime(4);
  EXPECT_NE(c1, c2);
  EXPECT_LT(c1, c2);
  auto const empty = LifecycleRuleCondition{};
  EXPECT_NE(c1, empty);
}

/// @test Verify that LifecycleRule::NoncurrentTimeBefore() works as expected.
TEST(LifecycleRuleTest, NoncurrentTimeBefore) {
  auto const c1 =
      LifecycleRule::NoncurrentTimeBefore(absl::CivilDay(2020, 7, 22));
  ASSERT_TRUE(c1.noncurrent_time_before.has_value());
  EXPECT_EQ(c1, c1);
  auto const c2 =
      LifecycleRule::NoncurrentTimeBefore(absl::CivilDay(2020, 7, 23));
  ASSERT_TRUE(c2.noncurrent_time_before.has_value());
  EXPECT_EQ(c2, c2);

  EXPECT_NE(c1, c2);
  EXPECT_LT(c1, c2);

  auto const empty = LifecycleRuleCondition{};
  EXPECT_NE(c1, empty);
}

/// @test Verify that LifecycleRule::DaysSinceCustomTime() works as expected.
TEST(LifecycleRuleTest, DaysSinceCustomTime) {
  auto const c1 = LifecycleRule::DaysSinceCustomTime(3);
  ASSERT_TRUE(c1.days_since_custom_time.has_value());
  EXPECT_EQ(3, c1.days_since_custom_time.value());
  EXPECT_EQ(c1, c1);
  auto const c2 = LifecycleRule::DaysSinceCustomTime(4);
  EXPECT_NE(c1, c2);
  EXPECT_LT(c1, c2);
  auto const empty = LifecycleRuleCondition{};
  EXPECT_NE(c1, empty);
}

/// @test Verify that LifecycleRule::NoncurrentTimeBefore() works as expected.
TEST(LifecycleRuleTest, CustomTimeBefore) {
  auto const c1 = LifecycleRule::CustomTimeBefore(absl::CivilDay(2020, 7, 23));
  ASSERT_TRUE(c1.custom_time_before.has_value());
  EXPECT_EQ(c1, c1);
  auto const c2 = LifecycleRule::CustomTimeBefore(absl::CivilDay(2020, 7, 24));
  ASSERT_TRUE(c2.custom_time_before.has_value());
  EXPECT_EQ(c2, c2);

  EXPECT_NE(c1, c2);
  EXPECT_LT(c1, c2);

  auto const empty = LifecycleRuleCondition{};
  EXPECT_NE(c1, empty);
}

/// @test Verify that LifecycleRule::MatchesPrefix works as expected.
TEST(LifecycleRuleTest, MatchesPrefix) {
  auto condition = LifecycleRule::MatchesPrefix("foo");
  ASSERT_TRUE(condition.matches_prefix.has_value());
  EXPECT_THAT(*condition.matches_prefix, ElementsAre("foo"));
}

/// @test Verify that LifecycleRule::MatchesPrefixes works as expected.
TEST(LifecycleRuleTest, MatchesPrefixes) {
  auto condition = LifecycleRule::MatchesPrefixes({"foo", "bar"});
  ASSERT_TRUE(condition.matches_prefix.has_value());
  EXPECT_THAT(*condition.matches_prefix, ElementsAre("foo", "bar"));
}

/// @test Verify that LifecycleRule::MatchesSuffix works as expected.
TEST(LifecycleRuleTest, MatchesSuffix) {
  auto condition = LifecycleRule::MatchesSuffix("foo");
  ASSERT_TRUE(condition.matches_suffix.has_value());
  EXPECT_THAT(*condition.matches_suffix, ElementsAre("foo"));
}

/// @test Verify that LifecycleRule::MatchesSuffixes works as expected.
TEST(LifecycleRuleTest, MatchesSuffixes) {
  auto condition = LifecycleRule::MatchesSuffixes({"foo", "bar"});
  ASSERT_TRUE(condition.matches_suffix.has_value());
  EXPECT_THAT(*condition.matches_suffix, ElementsAre("foo", "bar"));
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
  auto c1 = LifecycleRule::CreatedBefore(absl::CivilDay(2018, 1, 8));
  auto c2 = LifecycleRule::CreatedBefore(absl::CivilDay(2018, 2, 8));
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
        EXPECT_THAT(ex.what(), HasSubstr("LifecycleRule"));
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
              UnorderedElementsAre(storage_class::Standard(),
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
TEST(LifecycleRuleTest, ConditionConjunctionDaysSinceNoncurrentTime) {
  auto const c1 = LifecycleRule::DaysSinceNoncurrentTime(7);
  auto const c2 = LifecycleRule::DaysSinceNoncurrentTime(42);
  auto const condition = LifecycleRule::ConditionConjunction(c1, c2);
  ASSERT_TRUE(condition.days_since_noncurrent_time.has_value());
  EXPECT_EQ(42, *condition.days_since_noncurrent_time);
}

/// @test Verify that LifecycleRule::ConditionConjunction() works as expected.
TEST(LifecycleRuleTest, ConditionConjunctionNoncurrentTimeBefore) {
  auto const c1 =
      LifecycleRule::NoncurrentTimeBefore(absl::CivilDay(2020, 7, 22));
  auto const c2 =
      LifecycleRule::NoncurrentTimeBefore(absl::CivilDay(2020, 7, 23));
  auto const condition = LifecycleRule::ConditionConjunction(c1, c2);
  ASSERT_TRUE(condition.noncurrent_time_before.has_value());
  EXPECT_EQ(absl::CivilDay(2020, 7, 22), *condition.noncurrent_time_before);
}

/// @test Verify that LifecycleRule::ConditionConjunction() works as expected.
TEST(LifecycleRuleTest, ConditionConjunctionDaysSinceCustomTime) {
  auto const c1 = LifecycleRule::DaysSinceCustomTime(7);
  auto const c2 = LifecycleRule::DaysSinceCustomTime(42);
  auto const condition = LifecycleRule::ConditionConjunction(c1, c2);
  ASSERT_TRUE(condition.days_since_custom_time.has_value());
  EXPECT_EQ(42, *condition.days_since_custom_time);
}

/// @test Verify that LifecycleRule::ConditionConjunction() works as expected.
TEST(LifecycleRuleTest, ConditionConjunctionCustomTimeBefore) {
  auto const c1 = LifecycleRule::CustomTimeBefore(absl::CivilDay(2020, 7, 23));
  auto const c2 = LifecycleRule::CustomTimeBefore(absl::CivilDay(2020, 7, 24));
  auto const condition = LifecycleRule::ConditionConjunction(c1, c2);
  ASSERT_TRUE(condition.custom_time_before.has_value());
  EXPECT_EQ(absl::CivilDay(2020, 7, 23), *condition.custom_time_before);
}

/// @test Verify that LifecycleRule::ConditionConjunction() works as expected.
TEST(LifecycleRuleTest, ConditionConjunctionMatchesPrefix) {
  auto c1 = LifecycleRule::MatchesPrefixes({"foo/", "bar/", "baz/"});
  auto c2 = LifecycleRule::MatchesPrefixes({"foo/", "bar/", "quux/"});
  auto condition = LifecycleRule::ConditionConjunction(c1, c2);
  ASSERT_TRUE(condition.matches_prefix.has_value());
  EXPECT_THAT(*condition.matches_prefix, UnorderedElementsAre("foo/", "bar/"));
}

/// @test Verify that LifecycleRule::ConditionConjunction() works as expected.
TEST(LifecycleRuleTest, ConditionConjunctionMatchesSuffix) {
  auto c1 = LifecycleRule::MatchesSuffixes({".foo", ".bar", ".baz"});
  auto c2 = LifecycleRule::MatchesSuffixes({".foo", ".bar", ".quux"});
  auto condition = LifecycleRule::ConditionConjunction(c1, c2);
  ASSERT_TRUE(condition.matches_suffix.has_value());
  EXPECT_THAT(*condition.matches_suffix, UnorderedElementsAre(".foo", ".bar"));
}

/// @test Verify that LifecycleRule::ConditionConjunction() works as expected.
TEST(LifecycleRuleTest, ConditionConjunctionMultiple) {
  auto c1 = LifecycleRule::NumNewerVersions(7);
  auto c2 = LifecycleRule::MaxAge(42);
  auto c3 = LifecycleRule::MatchesStorageClasses({storage_class::Nearline(),
                                                  storage_class::Standard(),
                                                  storage_class::Regional()});
  auto c4 = LifecycleRule::MatchesPrefixes({"foo/", "bar/"});
  auto c5 = LifecycleRule::MatchesSuffixes({".lz4", ".gz"});
  auto condition = LifecycleRule::ConditionConjunction(c1, c2, c3, c4, c5);
  ASSERT_TRUE(condition.age.has_value());
  EXPECT_EQ(42, *condition.age);
  EXPECT_FALSE(condition.created_before.has_value());
  EXPECT_FALSE(condition.is_live.has_value());
  ASSERT_TRUE(condition.num_newer_versions.has_value());
  EXPECT_EQ(7, *condition.num_newer_versions);
  ASSERT_TRUE(condition.matches_storage_class.has_value());
  EXPECT_THAT(
      *condition.matches_storage_class,
      UnorderedElementsAre(storage_class::Nearline(), storage_class::Standard(),
                           storage_class::Regional()));
  ASSERT_TRUE(condition.matches_prefix.has_value());
  EXPECT_THAT(*condition.matches_prefix, UnorderedElementsAre("foo/", "bar/"));
  ASSERT_TRUE(condition.matches_suffix.has_value());
  EXPECT_THAT(*condition.matches_suffix, UnorderedElementsAre(".lz4", ".gz"));
}

/// @test Verify that LifecycleRule parsing works as expected.
TEST(LifecycleRuleTest, Parsing) {
  // This function uses ParseFromString() to create the LifecycleRule.
  LifecycleRule actual = CreateLifecycleRuleForTest();
  LifecycleRuleCondition expected_condition =
      LifecycleRule::ConditionConjunction(
          LifecycleRule::MaxAge(42),
          LifecycleRule::CreatedBefore(absl::CivilDay(2018, 7, 23)),
          LifecycleRule::IsLive(true),
          LifecycleRule::MatchesStorageClassStandard(),
          LifecycleRule::NumNewerVersions(7),
          LifecycleRule::DaysSinceNoncurrentTime(3),
          LifecycleRule::NoncurrentTimeBefore(absl::CivilDay(2020, 7, 22)),
          LifecycleRule::DaysSinceCustomTime(30),
          LifecycleRule::CustomTimeBefore(absl::CivilDay(2020, 7, 23)),
          LifecycleRule::MatchesPrefixes({"foo/", "bar/"}),
          LifecycleRule::MatchesSuffixes({".lz4", ".gz"}));
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
  EXPECT_THAT(actual, HasSubstr("age=42"));
  EXPECT_THAT(actual, HasSubstr("NEARLINE"));
  EXPECT_THAT(actual, HasSubstr("days_since_custom_time="));
  EXPECT_THAT(actual, HasSubstr("custom_time_before="));
  EXPECT_THAT(actual, HasSubstr("matches_prefix=[foo/, bar/]"));
  EXPECT_THAT(actual, HasSubstr("matches_suffix=[.lz4, .gz]"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
