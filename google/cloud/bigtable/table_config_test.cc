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

#include "google/cloud/bigtable/table_config.h"
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gmock/gmock.h>

namespace btadmin = ::google::bigtable::admin::v2;
namespace bigtable = google::cloud::bigtable;

TEST(TableConfig, Simple) {
  bigtable::TableConfig config;
  EXPECT_TRUE(config.column_families().empty());
  EXPECT_TRUE(config.initial_splits().empty());
  EXPECT_EQ(bigtable::TableConfig::TIMESTAMP_GRANULARITY_UNSPECIFIED,
            config.timestamp_granularity());

  config.add_column_family("fam", bigtable::GcRule::MaxNumVersions(2));
  config.add_initial_split("foo");
  config.add_initial_split("qux");
  config.set_timestamp_granularity(bigtable::TableConfig::MILLIS);

  auto const& families = config.column_families();
  auto it = families.find("fam");
  ASSERT_NE(families.end(), it);
  EXPECT_EQ(2, it->second.as_proto().max_num_versions());
  auto const& splits = config.initial_splits();
  ASSERT_EQ(2UL, splits.size());
  EXPECT_EQ("foo", splits[0]);
  EXPECT_EQ("qux", splits[1]);

  std::string expected_text = R"""(
table {
  column_families {
    key: 'fam'
    value { gc_rule { max_num_versions: 2 }}
  }
  granularity: MILLIS
}
initial_splits { key: 'foo' }
initial_splits { key: 'qux' }
)""";
  btadmin::CreateTableRequest expected;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(expected_text, &expected));

  auto request = std::move(config).as_proto();

  std::string delta;
  google::protobuf::util::MessageDifferencer differencer;
  differencer.ReportDifferencesToString(&delta);
  EXPECT_TRUE(differencer.Compare(expected, request)) << delta;
}

TEST(TableConfig, ComplexConstructor) {
  bigtable::TableConfig config({{"fam", bigtable::GcRule::MaxNumVersions(3)}},
                               {"foo", "qux"});

  auto const& families = config.column_families();
  auto it = families.find("fam");
  ASSERT_NE(families.end(), it);
  EXPECT_EQ(3, it->second.as_proto().max_num_versions());
  auto const& splits = config.initial_splits();
  ASSERT_EQ(2UL, splits.size());
  EXPECT_EQ("foo", splits[0]);
  EXPECT_EQ("qux", splits[1]);
}
