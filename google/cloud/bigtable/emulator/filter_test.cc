// Copyright 2024 Google LLC
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

#include "google/cloud/bigtable/emulator/filter.h"
#include "google/cloud/bigtable/data_connection.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <re2/re2.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

using ::google::bigtable::v2::RowFilter;
using ::testing::Return;
using testing_util::StatusIs;
using testing_util::chrono_literals::operator""_ms;

class MockStream : public AbstractCellStreamImpl {
 public:
  MOCK_METHOD(bool, ApplyFilter, (InternalFilter const& internal_filter),
              (override));
  MOCK_METHOD(bool, HasValue, (), (const, override));
  MOCK_METHOD(CellView const&, Value, (), (const, override));
  MOCK_METHOD(bool, Next, (NextMode mode), (override));
};

TEST(CellStream, NextAllSupported) {
  {
    auto mock_impl = std::make_unique<MockStream>();
    EXPECT_CALL(*mock_impl, Next(NextMode::kCell)).WillOnce(Return(true));
    CellStream(std::move(mock_impl)).Next();
  }
  {
    auto mock_impl = std::make_unique<MockStream>();
    EXPECT_CALL(*mock_impl, Next(NextMode::kColumn)).WillOnce(Return(true));
    CellStream(std::move(mock_impl)).Next(NextMode::kColumn);
  }
  {
    auto mock_impl = std::make_unique<MockStream>();
    EXPECT_CALL(*mock_impl, Next(NextMode::kRow)).WillOnce(Return(true));
    CellStream(std::move(mock_impl)).Next(NextMode::kRow);
  }
}

class TestCell {
 public:
  TestCell(std::string row_key, std::string column_family,
           std::string column_qualifier, std::chrono::milliseconds timestamp,
           std::string value)
      : row_key_(std::move(row_key)),
        column_family_(std::move(column_family)),
        column_qualifier_(std::move(column_qualifier)),
        timestamp_(std::move(timestamp)),
        value_(std::move(value)),
        view_(row_key_, column_family_, column_qualifier_, timestamp_, value_) {
  }

  TestCell(TestCell const& other)
      : row_key_(other.row_key_),
        column_family_(other.column_family_),
        column_qualifier_(other.column_qualifier_),
        timestamp_(other.timestamp_),
        value_(other.value_),
        view_(row_key_, column_family_, column_qualifier_, timestamp_, value_) {
  }
  TestCell(TestCell&& other) noexcept
      : row_key_(std::move(other.row_key_)),
        column_family_(std::move(other.column_family_)),
        column_qualifier_(std::move(other.column_qualifier_)),
        timestamp_(std::move(other.timestamp_)),
        value_(std::move(other.value_)),
        view_(row_key_, column_family_, column_qualifier_, timestamp_, value_) {
  }

  CellView const& AsCellView() const { return view_; }

  bool operator==(CellView const& cell_view) const {
    return row_key_ == cell_view.row_key() &&
           column_family_ == cell_view.column_family() &&
           column_qualifier_ == cell_view.column_qualifier() &&
           timestamp_ == cell_view.timestamp() && value_ == cell_view.value();
  }

 private:
  std::string row_key_;
  std::string column_family_;
  std::string column_qualifier_;
  std::chrono::milliseconds timestamp_;
  std::string value_;
  CellView view_;
};

std::ostream& operator<<(std::ostream& stream, TestCell const& test_cell) {
  auto const& cell_view = test_cell.AsCellView();
  stream << "Cell(" << cell_view.row_key() << " " << cell_view.column_family()
         << ":" << cell_view.column_qualifier() << " @"
         << cell_view.timestamp().count() << "ms: " << cell_view.value() << ")";
  return stream;
}

TEST(CellStream, NextColumnNotSupportedNoMoreData) {
  std::vector<TestCell> cells{TestCell{"row1", "cf1", "col1", 0_ms, "val1"}};
  auto cur_cell = cells.begin();

  auto mock_impl = std::make_unique<MockStream>();
  EXPECT_CALL(*mock_impl, Next(NextMode::kColumn)).WillOnce(Return(false));
  EXPECT_CALL(*mock_impl, Value).WillRepeatedly([&]() -> CellView const& {
    return cur_cell->AsCellView();
  });
  EXPECT_CALL(*mock_impl, HasValue).WillRepeatedly([&] {
    return cur_cell != cells.end();
  });
  EXPECT_CALL(*mock_impl, Next(NextMode::kCell)).WillOnce([&] {
    ++cur_cell;
    return true;
  });
  CellStream cell_stream(std::move(mock_impl));
  ASSERT_TRUE(cell_stream.HasValue());
  EXPECT_EQ(cells[0], cell_stream.Value());
  cell_stream.Next(NextMode::kColumn);
  ASSERT_FALSE(cell_stream.HasValue());
}

TEST(CellStream, NextColumnNotSupported) {
  std::vector<TestCell> cells{
      TestCell{"row1", "cf1", "col1", 0_ms, "val1"},
      TestCell{"row1", "cf1", "col1", 1_ms, "val2"},
      TestCell{"row1", "cf1", "col2", 0_ms, "val3"},  // column changed
      TestCell{"row1", "cf1", "col2", 1_ms, "val4"},
      TestCell{"row1", "cf2", "col2", 0_ms, "val5"},  // column family changed
      TestCell{"row1", "cf2", "col2", 1_ms, "val6"},
      TestCell{"row2", "cf2", "col2", 0_ms, "val7"},  // row changed
      TestCell{"row2", "cf2", "col2", 1_ms, "val8"}};
  auto cur_cell = cells.begin();

  auto mock_impl = std::make_unique<MockStream>();
  EXPECT_CALL(*mock_impl, Next(NextMode::kColumn))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_impl, Value).WillRepeatedly([&]() -> CellView const& {
    return cur_cell->AsCellView();
  });
  EXPECT_CALL(*mock_impl, HasValue).WillRepeatedly([&] {
    return cur_cell != cells.end();
  });
  EXPECT_CALL(*mock_impl, Next(NextMode::kCell)).WillRepeatedly([&] {
    ++cur_cell;
    return true;
  });

  CellStream cell_stream(std::move(mock_impl));

  cell_stream.Next(NextMode::kColumn);
  ASSERT_TRUE(cell_stream.HasValue());
  EXPECT_EQ(cells[2], cell_stream.Value());

  cell_stream.Next(NextMode::kColumn);
  ASSERT_TRUE(cell_stream.HasValue());
  EXPECT_EQ(cells[4], cell_stream.Value());

  cell_stream.Next(NextMode::kColumn);
  ASSERT_TRUE(cell_stream.HasValue());
  EXPECT_EQ(cells[6], cell_stream.Value());

  cell_stream.Next(NextMode::kColumn);
  ASSERT_FALSE(cell_stream.HasValue());
}

TEST(CellStream, NextRowNotSupported) {
  std::vector<TestCell> cells{
      TestCell{"row1", "cf1", "col1", 0_ms, "val1"},
      TestCell{"row1", "cf1", "col1", 1_ms, "val2"},
      TestCell{"row1", "cf1", "col2", 0_ms, "val3"},  // column changed
      TestCell{"row1", "cf1", "col2", 1_ms, "val4"},
      TestCell{"row1", "cf2", "col2", 0_ms, "val5"},  // column family changed
      TestCell{"row1", "cf2", "col2", 1_ms, "val6"},
      TestCell{"row2", "cf2", "col2", 0_ms, "val7"},  // row changed
      TestCell{"row2", "cf2", "col2", 1_ms, "val8"}};
  auto cur_cell = cells.begin();

  auto mock_impl = std::make_unique<MockStream>();
  EXPECT_CALL(*mock_impl, Next(NextMode::kColumn))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_impl, Value).WillRepeatedly([&]() -> CellView const& {
    return cur_cell->AsCellView();
  });
  EXPECT_CALL(*mock_impl, HasValue).WillRepeatedly([&] {
    return cur_cell != cells.end();
  });
  EXPECT_CALL(*mock_impl, Next(NextMode::kCell)).WillRepeatedly([&] {
    ++cur_cell;
    return true;
  });

  CellStream cell_stream(std::move(mock_impl));

  cell_stream.Next(NextMode::kColumn);
  ASSERT_TRUE(cell_stream.HasValue());
  EXPECT_EQ(cells[2], cell_stream.Value());

  cell_stream.Next(NextMode::kColumn);
  ASSERT_TRUE(cell_stream.HasValue());
  EXPECT_EQ(cells[4], cell_stream.Value());

  cell_stream.Next(NextMode::kColumn);
  ASSERT_TRUE(cell_stream.HasValue());
  EXPECT_EQ(cells[6], cell_stream.Value());

  cell_stream.Next(NextMode::kColumn);
  ASSERT_FALSE(cell_stream.HasValue());
}

TEST(CellStream, NextRowUnsupported) {
  std::vector<TestCell> cells{
      TestCell{"row1", "cf1", "col1", 0_ms, "val1"},
      TestCell{"row1", "cf1", "col1", 1_ms, "val2"},
      TestCell{"row1", "cf1", "col2", 0_ms, "val3"},  // column changed
      TestCell{"row1", "cf1", "col2", 1_ms, "val4"},
      TestCell{"row1", "cf2", "col2", 0_ms, "val5"},  // column family changed
      TestCell{"row1", "cf2", "col2", 1_ms, "val6"},
      TestCell{"row2", "cf2", "col2", 0_ms, "val7"},  // row changed
      TestCell{"row2", "cf2", "col2", 1_ms, "val8"}};
  auto cur_cell = cells.begin();

  auto mock_impl = std::make_unique<MockStream>();
  EXPECT_CALL(*mock_impl, Next(NextMode::kRow)).WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_impl, Value).WillRepeatedly([&]() -> CellView const& {
    return cur_cell->AsCellView();
  });
  EXPECT_CALL(*mock_impl, HasValue).WillRepeatedly([&] {
    return cur_cell != cells.end();
  });
  EXPECT_CALL(*mock_impl, Next(NextMode::kColumn)).WillRepeatedly([&] {
    cur_cell = std::find_if(cur_cell, cells.end(), [&](TestCell const& cell) {
      return cell.AsCellView().row_key() != cur_cell->AsCellView().row_key() ||
             cell.AsCellView().column_family() !=
                 cur_cell->AsCellView().column_family() ||
             cell.AsCellView().column_qualifier() !=
                 cur_cell->AsCellView().column_qualifier();
    });
    return true;
  });

  CellStream cell_stream(std::move(mock_impl));

  cell_stream.Next(NextMode::kRow);
  ASSERT_TRUE(cell_stream.HasValue());
  EXPECT_EQ(cells[6], cell_stream.Value());

  cell_stream.Next(NextMode::kRow);
  ASSERT_FALSE(cell_stream.HasValue());
}

TEST(CellStream, NextRowAndColumnUnsupported) {
  std::vector<TestCell> cells{
      TestCell{"row1", "cf1", "col1", 0_ms, "val1"},
      TestCell{"row1", "cf1", "col1", 1_ms, "val2"},
      TestCell{"row1", "cf1", "col2", 0_ms, "val3"},  // column changed
      TestCell{"row1", "cf1", "col2", 1_ms, "val4"},
      TestCell{"row1", "cf2", "col2", 0_ms, "val5"},  // column family changed
      TestCell{"row1", "cf2", "col2", 1_ms, "val6"},
      TestCell{"row2", "cf2", "col2", 0_ms, "val7"},  // row changed
      TestCell{"row2", "cf2", "col2", 1_ms, "val8"}};
  auto cur_cell = cells.begin();

  auto mock_impl = std::make_unique<MockStream>();
  EXPECT_CALL(*mock_impl, Next(NextMode::kRow)).WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_impl, Next(NextMode::kColumn))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_impl, Next(NextMode::kCell)).WillRepeatedly([&] {
    ++cur_cell;
    return true;
  });
  EXPECT_CALL(*mock_impl, Value).WillRepeatedly([&]() -> CellView const& {
    return cur_cell->AsCellView();
  });
  EXPECT_CALL(*mock_impl, HasValue).WillRepeatedly([&] {
    return cur_cell != cells.end();
  });

  CellStream cell_stream(std::move(mock_impl));

  cell_stream.Next(NextMode::kRow);
  ASSERT_TRUE(cell_stream.HasValue());
  EXPECT_EQ(cells[6], cell_stream.Value());

  cell_stream.Next(NextMode::kRow);
  ASSERT_FALSE(cell_stream.HasValue());
}

class CellStreamOrderTest : public ::testing::Test,
                            public ::testing::WithParamInterface<
                                // Expectation, lhs, rhs.
                                std::tuple<bool, TestCell, TestCell>> {};

INSTANTIATE_TEST_SUITE_P(
    , CellStreamOrderTest,
    ::testing::Values(
        std::make_tuple(false, TestCell{"row1", "cf1", "col1", 0_ms, "val1"},
                        TestCell{"row1", "cf1", "col1", 0_ms, "val1"}),
        std::make_tuple(false, TestCell{"row0", "cf1", "col1", 0_ms, "val1"},
                        TestCell{"row1", "cf1", "col1", 0_ms, "val1"}),
        std::make_tuple(true, TestCell{"row2", "cf1", "col1", 0_ms, "val1"},
                        TestCell{"row1", "cf1", "col1", 0_ms, "val1"}),
        std::make_tuple(false, TestCell{"row", "cf1", "col1", 0_ms, "val1"},
                        TestCell{"row", "cf1", "col1", 0_ms, "val1"}),
        std::make_tuple(false, TestCell{"row", "cf0", "col1", 0_ms, "val1"},
                        TestCell{"row", "cf1", "col1", 0_ms, "val1"}),
        std::make_tuple(true, TestCell{"row2", "cf2", "col1", 0_ms, "val1"},
                        TestCell{"row", "cf1", "col1", 0_ms, "val1"}),
        std::make_tuple(false, TestCell{"row", "cf", "col1", 0_ms, "val1"},
                        TestCell{"row", "cf", "col1", 0_ms, "val1"}),
        std::make_tuple(false, TestCell{"row", "cf", "col0", 0_ms, "val1"},
                        TestCell{"row", "cf", "col1", 0_ms, "val1"}),
        std::make_tuple(true, TestCell{"row", "cf", "col2", 0_ms, "val1"},
                        TestCell{"row", "cf", "col1", 0_ms, "val1"}),
        std::make_tuple(false, TestCell{"row", "cf", "col", 0_ms, "val1"},
                        TestCell{"row", "cf", "col", 0_ms, "val1"}),
        std::make_tuple(false, TestCell{"row", "cf", "col", 0_ms, "val1"},
                        TestCell{"row", "cf", "col", 1_ms, "val1"}),
        std::make_tuple(true, TestCell{"row", "cf", "col", 1_ms, "val1"},
                        TestCell{"row", "cf", "col", 0_ms, "val1"})));

TEST_P(CellStreamOrderTest, Order) {
  auto mock_impl_left = std::make_unique<MockStream>();
  auto left_cell = std::get<1>(GetParam());
  auto right_cell = std::get<2>(GetParam());
  EXPECT_CALL(*mock_impl_left, Value).WillRepeatedly([&]() -> CellView const& {
    return left_cell.AsCellView();
  });
  EXPECT_CALL(*mock_impl_left, HasValue).WillRepeatedly([&] { return true; });

  auto mock_impl_right = std::make_unique<MockStream>();
  EXPECT_CALL(*mock_impl_right, Value).WillRepeatedly([&]() -> CellView const& {
    return right_cell.AsCellView();
  });
  EXPECT_CALL(*mock_impl_right, HasValue).WillRepeatedly([&] { return true; });
  auto left = std::make_unique<CellStream>(std::move(mock_impl_left));
  auto right = std::make_unique<CellStream>(std::move(mock_impl_right));
  EXPECT_EQ(std::get<0>(GetParam()),
            MergeCellStreams::CellStreamGreater()(left, right));
}

TEST(MergeCellStreams, NoStreams) {
  CellStream stream(
      std::make_unique<MergeCellStreams>(std::vector<CellStream>{}));
  EXPECT_FALSE(stream.HasValue());
}

TEST(MergeCellStreams, OnlyEmptyStreams) {
  auto empty_impl_1 = std::make_unique<MockStream>();
  EXPECT_CALL(*empty_impl_1, HasValue).WillRepeatedly(Return(false));
  auto empty_impl_2 = std::make_unique<MockStream>();
  EXPECT_CALL(*empty_impl_2, HasValue).WillRepeatedly(Return(false));
  CellStream empty_1(std::move(empty_impl_1));
  CellStream empty_2(std::move(empty_impl_2));
  std::vector<CellStream> streams;
  streams.emplace_back(std::move(empty_1));
  streams.emplace_back(std::move(empty_2));
  CellStream stream(std::make_unique<MergeCellStreams>(std::move(streams)));
  EXPECT_FALSE(stream.HasValue());
}

TEST(MergeCellStreams, OneStream) {
  std::vector<TestCell> cells{
      TestCell{"row1", "cf1", "col1", 0_ms, "val1"},
      TestCell{"row1", "cf1", "col1", 1_ms, "val2"},
      TestCell{"row1", "cf1", "col2", 0_ms, "val3"},  // column changed
      TestCell{"row1", "cf1", "col2", 1_ms, "val4"},
      TestCell{"row1", "cf2", "col2", 0_ms, "val5"},  // column family changed
      TestCell{"row1", "cf2", "col2", 1_ms, "val6"},
      TestCell{"row2", "cf2", "col2", 0_ms, "val7"},  // row changed
      TestCell{"row2", "cf2", "col2", 1_ms, "val8"}};

  auto cur_cell = cells.begin();

  auto mock_impl = std::make_unique<MockStream>();
  EXPECT_CALL(*mock_impl, Next(NextMode::kColumn)).WillOnce([&]() {
    cur_cell = std::next(cells.begin(), 2);
    return true;
  });
  EXPECT_CALL(*mock_impl, Next(NextMode::kRow)).WillOnce([&]() {
    cur_cell = std::next(cells.begin(), 6);
    return true;
  });
  EXPECT_CALL(*mock_impl, Next(NextMode::kCell)).WillRepeatedly([&]() {
    ++cur_cell;
    return true;
  });
  EXPECT_CALL(*mock_impl, Value).WillRepeatedly([&]() -> CellView const& {
    return cur_cell->AsCellView();
  });
  EXPECT_CALL(*mock_impl, HasValue).WillRepeatedly([&] {
    return cur_cell != cells.end();
  });

  std::vector<CellStream> streams;
  streams.emplace_back(std::move(mock_impl));
  CellStream stream(std::make_unique<MergeCellStreams>(std::move(streams)));

  ASSERT_TRUE(stream.HasValue());
  EXPECT_EQ(cells[0], stream.Value());

  stream.Next(NextMode::kColumn);
  ASSERT_TRUE(stream.HasValue());
  EXPECT_EQ(cells[2], stream.Value());

  stream.Next(NextMode::kRow);
  ASSERT_TRUE(stream.HasValue());
  EXPECT_EQ(cells[6], stream.Value());

  stream.Next(NextMode::kCell);
  ASSERT_TRUE(stream.HasValue());
  EXPECT_EQ(cells[7], stream.Value());

  stream.Next(NextMode::kCell);
  ASSERT_FALSE(stream.HasValue());
}

struct TestStreamData {
  explicit TestStreamData(std::vector<TestCell> data)
      : cells(std::move(data)),
        cur_cell(cells.begin()),
        stream(std::make_unique<MockStream>()) {}

  std::vector<TestCell> cells;
  std::vector<TestCell>::iterator cur_cell;
  std::unique_ptr<MockStream> stream;
};

TEST(MergeCellStreams, ThreeStreams) {
  TestStreamData stream_data_1(
      std::vector<TestCell>{TestCell{"row1", "cf1", "col1", 0_ms, "val1"},
                            TestCell{"row1", "cf2", "col1", 2_ms, "val2"}});

  TestStreamData stream_data_2(
      std::vector<TestCell>{TestCell{"row1", "cf1", "col1", 1_ms, "val1"},
                            TestCell{"row2", "cf1", "col1", 1_ms, "val2"},
                            TestCell{"row2", "cf1", "col2", 0_ms, "val3"}});

  TestStreamData stream_data_3(
      std::vector<TestCell>{TestCell{"row1", "cf1", "col1", 3_ms, "val1"},
                            TestCell{"row2", "cf0", "col1", 1_ms, "val2"}});

  auto prepare_stream = [](TestStreamData& stream_data) {
    EXPECT_CALL(*stream_data.stream, Next(NextMode::kCell))
        .WillRepeatedly([&]() {
          ++stream_data.cur_cell;
          return true;
        });
    EXPECT_CALL(*stream_data.stream, Value)
        .WillRepeatedly([&]() -> CellView const& {
          return stream_data.cur_cell->AsCellView();
        });
    EXPECT_CALL(*stream_data.stream, HasValue).WillRepeatedly([&] {
      return stream_data.cur_cell != stream_data.cells.end();
    });
    return CellStream(std::move(stream_data.stream));
  };

  std::vector<CellStream> streams;
  streams.push_back(prepare_stream(stream_data_1));
  streams.push_back(prepare_stream(stream_data_2));
  streams.push_back(prepare_stream(stream_data_3));
  CellStream stream(std::make_unique<MergeCellStreams>(std::move(streams)));

  ASSERT_TRUE(stream.HasValue());
  EXPECT_EQ(stream_data_1.cells[0], stream.Value());

  stream.Next(NextMode::kCell);
  ASSERT_TRUE(stream.HasValue());
  EXPECT_EQ(stream_data_2.cells[0], stream.Value());

  stream.Next(NextMode::kCell);
  ASSERT_TRUE(stream.HasValue());
  EXPECT_EQ(stream_data_3.cells[0], stream.Value());

  stream.Next(NextMode::kCell);
  ASSERT_TRUE(stream.HasValue());
  EXPECT_EQ(stream_data_1.cells[1], stream.Value());

  stream.Next(NextMode::kCell);
  ASSERT_TRUE(stream.HasValue());
  EXPECT_EQ(stream_data_3.cells[1], stream.Value());

  stream.Next(NextMode::kCell);
  ASSERT_TRUE(stream.HasValue());
  EXPECT_EQ(stream_data_2.cells[1], stream.Value());

  stream.Next(NextMode::kCell);
  ASSERT_TRUE(stream.HasValue());
  EXPECT_EQ(stream_data_2.cells[2], stream.Value());

  stream.Next(NextMode::kCell);
  ASSERT_FALSE(stream.HasValue());
}

TEST(MergeCellStreams, AdvancingRowAdvancesAllRelevantStreams) {
  // When calling Next(NextMode::kRow), all streams currently pointing to the
  // same row as the first stream should be advanced.
  TestStreamData stream_data_1(
      std::vector<TestCell>{TestCell{"row1", "cf1", "col1", 0_ms, "val1"},
                            TestCell{"row2", "cf2", "col1", 2_ms, "val2"}});

  TestStreamData stream_data_2(
      std::vector<TestCell>{TestCell{"row2", "cf1", "col1", 1_ms, "val2"},
                            TestCell{"row2", "cf1", "col2", 10_ms, "val3"}});

  TestStreamData stream_data_3(
      std::vector<TestCell>{TestCell{"row1", "cf1", "col1", 3_ms, "val1"},
                            TestCell{"row2", "cf0", "col1", 1_ms, "val2"}});

  TestStreamData stream_data_4(
      std::vector<TestCell>{TestCell{"row1", "cf1", "col1", 3_ms, "val1"}});

  auto prepare_stream = [](TestStreamData& stream_data) {
    EXPECT_CALL(*stream_data.stream, Value)
        .WillRepeatedly([&]() -> CellView const& {
          return stream_data.cur_cell->AsCellView();
        });
    EXPECT_CALL(*stream_data.stream, HasValue).WillRepeatedly([&] {
      return stream_data.cur_cell != stream_data.cells.end();
    });
  };
  prepare_stream(stream_data_1);
  prepare_stream(stream_data_2);
  prepare_stream(stream_data_3);
  prepare_stream(stream_data_4);

  EXPECT_CALL(*stream_data_1.stream, Next(NextMode::kRow)).WillOnce([&]() {
    stream_data_1.cur_cell = std::next(stream_data_1.cells.begin());
    return true;
  });
  EXPECT_CALL(*stream_data_3.stream, Next(NextMode::kRow)).WillOnce([&]() {
    stream_data_3.cur_cell = std::next(stream_data_3.cells.begin());
    return true;
  });
  EXPECT_CALL(*stream_data_4.stream, Next(NextMode::kRow)).WillOnce([&]() {
    stream_data_4.cur_cell = stream_data_4.cells.end();
    return true;
  });

  EXPECT_CALL(*stream_data_1.stream, Next(NextMode::kCell)).WillOnce([&]() {
    ++stream_data_1.cur_cell;
    return true;
  });

  EXPECT_CALL(*stream_data_2.stream, Next(NextMode::kCell))
      .Times(2)
      .WillRepeatedly([&]() {
        ++stream_data_2.cur_cell;
        return true;
      });

  EXPECT_CALL(*stream_data_3.stream, Next(NextMode::kCell)).WillOnce([&]() {
    ++stream_data_3.cur_cell;
    return true;
  });

  std::vector<CellStream> streams;
  streams.emplace_back(std::move(stream_data_1.stream));
  streams.emplace_back(std::move(stream_data_2.stream));
  streams.emplace_back(std::move(stream_data_3.stream));
  streams.emplace_back(std::move(stream_data_4.stream));
  CellStream stream(std::make_unique<MergeCellStreams>(std::move(streams)));

  ASSERT_TRUE(stream.HasValue());
  EXPECT_EQ(stream_data_1.cells[0], stream.Value());

  stream.Next(NextMode::kRow);
  ASSERT_TRUE(stream.HasValue());
  EXPECT_EQ(stream_data_3.cells[1], stream.Value());

  stream.Next(NextMode::kCell);
  ASSERT_TRUE(stream.HasValue());
  EXPECT_EQ(stream_data_2.cells[0], stream.Value());

  stream.Next(NextMode::kCell);
  ASSERT_TRUE(stream.HasValue());
  EXPECT_EQ(stream_data_2.cells[1], stream.Value());

  stream.Next(NextMode::kCell);
  ASSERT_TRUE(stream.HasValue());
  EXPECT_EQ(stream_data_1.cells[1], stream.Value());

  stream.Next(NextMode::kCell);
  ASSERT_FALSE(stream.HasValue());
}

TEST(MergeCellStreams, AdvancingColumnAdvancesAllRelevantStreams) {
  // When calling Next(NextMode::kColumn), all streams currently pointing to the
  // same column as the first stream should be advanced.
  TestStreamData stream_data(
      std::vector<TestCell>{TestCell{"row1", "cf1", "col1", 0_ms, "val1"},
                            TestCell{"row2", "cf2", "col1", 2_ms, "val2"}});

  TestStreamData stream_data_different_column_family(
      std::vector<TestCell>{TestCell{"row1", "cf2", "col1", 1_ms, "val2"}});

  TestStreamData stream_data_different_column_qualifier(
      std::vector<TestCell>{TestCell{"row1", "cf1", "col2", 1_ms, "val2"}});

  TestStreamData stream_data_different_row(
      std::vector<TestCell>{TestCell{"row2", "cf1", "col1", 1_ms, "val2"}});

  TestStreamData stream_data_same_column_different_timestamp(
      std::vector<TestCell>{TestCell{"row1", "cf1", "col1", 10_ms, "val2"}});

  auto prepare_stream = [](TestStreamData& stream_data) {
    EXPECT_CALL(*stream_data.stream, Value)
        .WillRepeatedly([&]() -> CellView const& {
          return stream_data.cur_cell->AsCellView();
        });
    EXPECT_CALL(*stream_data.stream, HasValue).WillRepeatedly([&] {
      return stream_data.cur_cell != stream_data.cells.end();
    });
  };
  prepare_stream(stream_data);
  prepare_stream(stream_data_different_column_family);
  prepare_stream(stream_data_different_column_qualifier);
  prepare_stream(stream_data_different_row);
  prepare_stream(stream_data_same_column_different_timestamp);

  EXPECT_CALL(*stream_data.stream, Next(NextMode::kColumn)).WillOnce([&]() {
    ++stream_data.cur_cell;
    return true;
  });
  EXPECT_CALL(*stream_data.stream, Next(NextMode::kCell)).WillOnce([&]() {
    ++stream_data.cur_cell;
    return true;
  });
  EXPECT_CALL(*stream_data_same_column_different_timestamp.stream,
              Next(NextMode::kColumn))
      .WillOnce([&]() {
        ++stream_data_same_column_different_timestamp.cur_cell;
        return true;
      });

  EXPECT_CALL(*stream_data_different_column_family.stream,
              Next(NextMode::kCell))
      .WillOnce([&]() {
        ++stream_data_different_column_family.cur_cell;
        return true;
      });

  EXPECT_CALL(*stream_data_different_column_qualifier.stream,
              Next(NextMode::kCell))
      .WillOnce([&]() {
        ++stream_data_different_column_qualifier.cur_cell;
        return true;
      });

  EXPECT_CALL(*stream_data_different_row.stream, Next(NextMode::kCell))
      .WillOnce([&]() {
        ++stream_data_different_row.cur_cell;
        return true;
      });

  std::vector<CellStream> streams;
  streams.emplace_back(std::move(stream_data.stream));
  streams.emplace_back(std::move(stream_data_different_column_family.stream));
  streams.emplace_back(
      std::move(stream_data_different_column_qualifier.stream));
  streams.emplace_back(std::move(stream_data_different_row.stream));
  streams.emplace_back(
      std::move(stream_data_same_column_different_timestamp.stream));
  CellStream stream(std::make_unique<MergeCellStreams>(std::move(streams)));

  ASSERT_TRUE(stream.HasValue());
  EXPECT_EQ(stream_data.cells[0], stream.Value());

  stream.Next(NextMode::kColumn);
  ASSERT_TRUE(stream.HasValue());
  EXPECT_EQ(stream_data_different_column_qualifier.cells[0], stream.Value());

  stream.Next(NextMode::kCell);
  ASSERT_TRUE(stream.HasValue());
  EXPECT_EQ(stream_data_different_column_family.cells[0], stream.Value());

  stream.Next(NextMode::kCell);
  ASSERT_TRUE(stream.HasValue());
  EXPECT_EQ(stream_data_different_row.cells[0], stream.Value());

  stream.Next(NextMode::kCell);
  ASSERT_TRUE(stream.HasValue());
  EXPECT_EQ(stream_data.cells[1], stream.Value());

  stream.Next(NextMode::kCell);
  ASSERT_FALSE(stream.HasValue());
}

class InvalidFilterProtoTest : public ::testing::Test {
 protected:
  ::google::bigtable::v2::RowFilter filter_;
  StatusOr<CellStream> TryCreate() {
    return CreateFilter(
        filter_, [] { return CellStream(std::make_unique<MockStream>()); });
  }
};

TEST_F(InvalidFilterProtoTest, PassAll) {
  filter_.set_pass_all_filter(false);
  auto maybe_stream = TryCreate();
  EXPECT_THAT(maybe_stream,
              StatusIs(StatusCode::kInvalidArgument,
                       testing::HasSubstr(
                           "`pass_all_filter` explicitly set to `false`")));
}

TEST_F(InvalidFilterProtoTest, BlockAll) {
  filter_.set_block_all_filter(false);
  auto maybe_stream = TryCreate();
  EXPECT_THAT(maybe_stream,
              StatusIs(StatusCode::kInvalidArgument,
                       testing::HasSubstr(
                           "`block_all_filter` explicitly set to `false`")));
}

TEST_F(InvalidFilterProtoTest, RowKeyRegex) {
  filter_.set_row_key_regex_filter("[");
  auto maybe_stream = TryCreate();
  EXPECT_THAT(maybe_stream,
              StatusIs(StatusCode::kInvalidArgument,
                       testing::HasSubstr(
                           "`row_key_regex_filter` is not a valid RE2 regex")));
}

TEST_F(InvalidFilterProtoTest, ValueRegex) {
  filter_.set_value_regex_filter("[");
  auto maybe_stream = TryCreate();
  EXPECT_THAT(maybe_stream,
              StatusIs(StatusCode::kInvalidArgument,
                       testing::HasSubstr(
                           "`value_regex_filter` is not a valid RE2 regex.")));
}

TEST_F(InvalidFilterProtoTest, RowSampleNegative) {
  filter_.set_row_sample_filter(-1);
  auto maybe_stream = TryCreate();
  EXPECT_THAT(maybe_stream,
              StatusIs(StatusCode::kInvalidArgument,
                       testing::HasSubstr(
                           "`row_sample_filter` is not a valid probability.")));
}

TEST_F(InvalidFilterProtoTest, RowSampleTooLarge) {
  filter_.set_row_sample_filter(10);
  auto maybe_stream = TryCreate();
  EXPECT_THAT(maybe_stream,
              StatusIs(StatusCode::kInvalidArgument,
                       testing::HasSubstr(
                           "`row_sample_filter` is not a valid probability.")));
}

TEST_F(InvalidFilterProtoTest, FamilyNameRegex) {
  filter_.set_family_name_regex_filter("[");
  auto maybe_stream = TryCreate();
  EXPECT_THAT(
      maybe_stream,
      StatusIs(StatusCode::kInvalidArgument,
               testing::HasSubstr(
                   "`family_name_regex_filter` is not a valid RE2 regex.")));
}

TEST_F(InvalidFilterProtoTest, ColumnQualifierRegex) {
  filter_.set_column_qualifier_regex_filter("[");
  auto maybe_stream = TryCreate();
  EXPECT_THAT(
      maybe_stream,
      StatusIs(
          StatusCode::kInvalidArgument,
          testing::HasSubstr(
              "`column_qualifier_regex_filter` is not a valid RE2 regex.")));
}

TEST_F(InvalidFilterProtoTest, PerRowOffset) {
  filter_.set_cells_per_row_offset_filter(-1);
  auto maybe_stream = TryCreate();
  EXPECT_THAT(maybe_stream,
              StatusIs(StatusCode::kInvalidArgument,
                       testing::HasSubstr(
                           "`cells_per_row_offset_filter` is negative.")));
}

TEST_F(InvalidFilterProtoTest, PerRowLimit) {
  filter_.set_cells_per_row_limit_filter(-1);
  auto maybe_stream = TryCreate();
  EXPECT_THAT(maybe_stream,
              StatusIs(StatusCode::kInvalidArgument,
                       testing::HasSubstr(
                           "`cells_per_row_limit_filter` is negative.")));
}

TEST_F(InvalidFilterProtoTest, PerColumnLimit) {
  filter_.set_cells_per_column_limit_filter(-1);
  auto maybe_stream = TryCreate();
  EXPECT_THAT(maybe_stream,
              StatusIs(StatusCode::kInvalidArgument,
                       testing::HasSubstr(
                           "`cells_per_column_limit_filter` is negative.")));
}

TEST_F(InvalidFilterProtoTest, StripValue) {
  filter_.set_strip_value_transformer(false);
  auto maybe_stream = TryCreate();
  EXPECT_THAT(
      maybe_stream,
      StatusIs(StatusCode::kInvalidArgument,
               testing::HasSubstr(
                   "`strip_value_transformer` explicitly set to `false`.")));
}

TEST_F(InvalidFilterProtoTest, ConditionNoPredicate) {
  filter_.mutable_condition();
  auto maybe_stream = TryCreate();
  EXPECT_THAT(maybe_stream,
              StatusIs(StatusCode::kInvalidArgument,
                       testing::HasSubstr(
                           "`condition` must have a `predicate_filter` set.")));
}

TEST_F(InvalidFilterProtoTest, SinkFalse) {
  filter_.set_sink(false);
  auto maybe_stream = TryCreate();
  EXPECT_THAT(
      maybe_stream,
      StatusIs(StatusCode::kInvalidArgument,
               testing::HasSubstr("`sink` explicitly set to `false`.")));
}

TEST_F(InvalidFilterProtoTest, ChainSinkFalse) {
  filter_.mutable_chain()->add_filters()->set_sink(false);
  auto maybe_stream = TryCreate();
  EXPECT_THAT(
      maybe_stream,
      StatusIs(StatusCode::kInvalidArgument,
               testing::HasSubstr("`sink` explicitly set to `false`.")));
}

TEST_F(InvalidFilterProtoTest, InterleaveSinkFalse) {
  filter_.mutable_interleave()->add_filters()->set_sink(false);
  auto maybe_stream = TryCreate();
  EXPECT_THAT(
      maybe_stream,
      StatusIs(StatusCode::kInvalidArgument,
               testing::HasSubstr("`sink` explicitly set to `false`.")));
}

TEST(FilterTest, BlockAll) {
  RowFilter filter;
  filter.set_block_all_filter(true);

  auto maybe_stream = CreateFilter(
      filter, [] { return CellStream(std::make_unique<MockStream>()); });

  ASSERT_STATUS_OK(maybe_stream);
  EXPECT_FALSE(maybe_stream->HasValue());
}

bool operator==(RowKeyRegex const& lhs, RowKeyRegex const& rhs) {
  return lhs.regex == rhs.regex;
}
bool operator==(FamilyNameRegex const& lhs, FamilyNameRegex const& rhs) {
  return lhs.regex == rhs.regex;
}
bool operator==(ColumnRegex const& lhs, ColumnRegex const& rhs) {
  return lhs.regex == rhs.regex;
}
bool operator==(ColumnRange const& lhs, ColumnRange const& rhs) {
  return lhs.range == rhs.range;
}
bool operator==(TimestampRange const& lhs, TimestampRange const& rhs) {
  return lhs.range == rhs.range;
}

class FilterPrinter {
 public:
  explicit FilterPrinter(std::ostream& stream) : stream_(stream) {}
  void operator()(RowKeyRegex const& to_print) const {
    stream_ << "RowKeyRegex(" << to_print.regex->pattern() << ")";
  }
  void operator()(FamilyNameRegex const& to_print) {
    stream_ << "FamilyNameRegex(" << to_print.regex->pattern() << ")";
  }
  void operator()(ColumnRegex const& to_print) {
    stream_ << "ColumnRegex(" << to_print.regex->pattern() << ")";
  }
  void operator()(ColumnRange const& to_print) {
    stream_ << "ColumnRange(" << to_print.column_family << "," << to_print.range
            << ")";
  }
  void operator()(TimestampRange const& to_print) {
    stream_ << "TimestampRange(" << to_print.range << ")";
  }

 private:
  std::ostream& stream_;
};

std::ostream& operator<<(std::ostream& os, InternalFilter const& filter) {
  absl::visit(FilterPrinter(os), filter);
  return os;
}

class FilterApplicationPropagation : public ::testing::Test {
 protected:
  struct InternalFilterType {
    InternalFilter internal_filter;
    bool should_propagate;
  };

  FilterApplicationPropagation()
      : sample_regex_(std::make_shared<re2::RE2>("foo.*")),
        sample_string_range_("a", true, "b", false),
        sample_ts_range_(std::chrono::milliseconds(10),
                         std::chrono::milliseconds(20)) {
    internal_filters_.emplace(
        "row_key_regex", InternalFilterType{RowKeyRegex{sample_regex_}, true});
    internal_filters_.emplace(
        "family_name_regex",
        InternalFilterType{FamilyNameRegex{sample_regex_}, true});
    internal_filters_.emplace(
        "column_regex", InternalFilterType{ColumnRegex{sample_regex_}, true});
    internal_filters_.emplace(
        "column_range",
        InternalFilterType{ColumnRange{"fam", sample_string_range_}, true});
    internal_filters_.emplace(
        "timestamp_range",
        InternalFilterType{TimestampRange{sample_ts_range_}, true});
  }

  void PropagationNotExpected(std::string const& filter_type) {
    auto filter_type_it = internal_filters_.find(filter_type);
    ASSERT_NE(internal_filters_.end(), filter_type_it);
    filter_type_it->second.should_propagate = false;
  }

  std::shared_ptr<re2::RE2> sample_regex_;
  StringRangeSet::Range sample_string_range_;
  TimestampRangeSet::Range sample_ts_range_;
  std::map<std::string, InternalFilterType> internal_filters_;

  void TestPropagation(RowFilter const& filter, int num_applies_to_ignore) {
    for (bool underlying_supports_filter : {false, true}) {
      for (auto const& internal_filter_type : internal_filters_) {
        auto maybe_stream = CreateFilter(filter, [&] {
          auto mock_impl = std::make_unique<MockStream>();
          if (num_applies_to_ignore) {
            // Creating the filter might trigger some `ApplyFilter` calls which
            // we're not interested in in this test. Let's ignore them.
            EXPECT_CALL(*mock_impl, ApplyFilter)
                .Times(num_applies_to_ignore)
                .WillRepeatedly(Return(false));
          }
          if (internal_filter_type.second.should_propagate) {
            EXPECT_CALL(
                *mock_impl,
                ApplyFilter(internal_filter_type.second.internal_filter))
                .WillOnce(Return(underlying_supports_filter));
          }
          return CellStream(std::move(mock_impl));
        });
        ASSERT_STATUS_OK(maybe_stream);

        if (underlying_supports_filter) {
          EXPECT_EQ(internal_filter_type.second.should_propagate,
                    maybe_stream->ApplyFilter(
                        internal_filter_type.second.internal_filter))
              << "for filter " << internal_filter_type.first;
        } else {
          EXPECT_FALSE(maybe_stream->ApplyFilter(
              internal_filter_type.second.internal_filter))
              << "for filter " << internal_filter_type.first;
        }
      }
    }
  }
};

TEST_F(FilterApplicationPropagation, PassAll) {
  RowFilter filter;
  filter.set_pass_all_filter(true);

  TestPropagation(filter, 0);
}

TEST_F(FilterApplicationPropagation, BlockAll) {
  RowFilter filter;
  filter.set_block_all_filter(true);

  for (auto& internal_filter : internal_filters_) {
    auto maybe_stream =
        CreateFilter(filter, [&] { return CellStream(nullptr); });
    ASSERT_STATUS_OK(maybe_stream);
    EXPECT_EQ(true,
              maybe_stream->ApplyFilter(internal_filter.second.internal_filter))
        << " for filter " << internal_filter.first;
  }
}

TEST_F(FilterApplicationPropagation, RowKeyRegex) {
  RowFilter filter;
  filter.set_row_key_regex_filter("foo.*");

  TestPropagation(filter, 1);
}

TEST_F(FilterApplicationPropagation, RowSample) {
  RowFilter filter;
  filter.set_row_sample_filter(0.5);

  TestPropagation(filter, 0);
}

TEST_F(FilterApplicationPropagation, FamilyNameRegex) {
  RowFilter filter;
  filter.set_family_name_regex_filter("foo.*");

  TestPropagation(filter, 1);
}

TEST_F(FilterApplicationPropagation, ColumnQualifierRegex) {
  RowFilter filter;
  filter.set_column_qualifier_regex_filter("foo.*");

  TestPropagation(filter, 1);
}

TEST_F(FilterApplicationPropagation, ColumnRange) {
  RowFilter filter;
  filter.mutable_column_range_filter()->set_family_name("fam1");
  filter.mutable_column_range_filter()->set_start_qualifier_open("q1");
  filter.mutable_column_range_filter()->set_end_qualifier_closed("q4");

  TestPropagation(filter, 1);
}

TEST_F(FilterApplicationPropagation, TimestampRange) {
  RowFilter filter;
  filter.mutable_timestamp_range_filter()->set_start_timestamp_micros(1000);
  filter.mutable_timestamp_range_filter()->set_end_timestamp_micros(2000);

  TestPropagation(filter, 1);
}

TEST_F(FilterApplicationPropagation, ValueRegex) {
  RowFilter filter;
  filter.set_value_regex_filter("foo.*");

  TestPropagation(filter, 0);
}

TEST_F(FilterApplicationPropagation, ValueRange) {
  RowFilter filter;
  filter.mutable_value_range_filter()->set_start_value_open("q1");
  filter.mutable_value_range_filter()->set_end_value_closed("q4");

  TestPropagation(filter, 0);
}

TEST_F(FilterApplicationPropagation, PerRowOffset) {
  RowFilter filter;
  filter.set_cells_per_row_offset_filter(10);

  for (auto const& filter_type : {"family_name_regex", "column_regex",
                                  "column_range", "timestamp_range"}) {
    PropagationNotExpected(filter_type);
  }

  TestPropagation(filter, 0);
}

TEST_F(FilterApplicationPropagation, PerRowLimit) {
  RowFilter filter;
  filter.set_cells_per_row_limit_filter(10);

  for (auto const& filter_type : {"family_name_regex", "column_regex",
                                  "column_range", "timestamp_range"}) {
    PropagationNotExpected(filter_type);
  }

  TestPropagation(filter, 0);
}

TEST_F(FilterApplicationPropagation, PerColumnLimit) {
  RowFilter filter;
  filter.set_cells_per_column_limit_filter(10);

  PropagationNotExpected("timestamp_range");

  TestPropagation(filter, 0);
}

TEST_F(FilterApplicationPropagation, StripValue) {
  RowFilter filter;
  filter.set_strip_value_transformer(true);

  TestPropagation(filter, 0);
}

TEST_F(FilterApplicationPropagation, ApplyLabel) {
  RowFilter filter;
  filter.set_apply_label_transformer("foo");

  TestPropagation(filter, 0);
}

TEST_F(FilterApplicationPropagation, InterleaveAllSupport) {
  RowFilter filter;
  auto& interleave = *filter.mutable_interleave();
  interleave.add_filters()->set_pass_all_filter(true);
  interleave.add_filters()->set_pass_all_filter(true);

  TestPropagation(filter, 0);
}

TEST_F(FilterApplicationPropagation, Condition) {
  RowFilter filter;
  auto& condition = *filter.mutable_condition();
  condition.mutable_predicate_filter()->set_pass_all_filter(true);
  condition.mutable_true_filter()->set_pass_all_filter(true);
  condition.mutable_false_filter()->set_pass_all_filter(true);

  for (bool underlying_supports_filter : {false, true}) {
    for (auto& internal_filter_type : internal_filters_) {
      // For lack of a better idea this test relies on the fact that the
      // implementation calls the mocked source stream ctor in the following
      // order:
      // * for the source data
      // * for the predicate stream
      // * for the true branch stream
      // * for the false branch stream
      std::int32_t num_streams_created = 0;
      auto maybe_stream = CreateFilter(filter, [&] {
        auto mock_impl = std::make_unique<MockStream>();
        if (num_streams_created < 2 &&
            internal_filter_type.first == "row_key_regex") {
          // source or predicate stream - they should only pass the row regexes
          EXPECT_CALL(*mock_impl,
                      ApplyFilter(internal_filter_type.second.internal_filter))
              .WillOnce(Return(false));  // this should have no effect on the
                                         // result.
        }
        if (num_streams_created >= 2) {
          // true or false branch stream - they should propagate all filters
          if (internal_filter_type.second.should_propagate) {
            EXPECT_CALL(
                *mock_impl,
                ApplyFilter(internal_filter_type.second.internal_filter))
                .WillOnce(Return(underlying_supports_filter));
          }
        }
        ++num_streams_created;
        return CellStream(std::move(mock_impl));
      });
      ASSERT_STATUS_OK(maybe_stream);
      EXPECT_EQ(underlying_supports_filter,
                maybe_stream->ApplyFilter(
                    internal_filter_type.second.internal_filter))
          << " for filter " << internal_filter_type.first;
    }
  }
}
class InternalFiltersAreApplied : public ::testing::Test {
 protected:
  RowFilter filter_;

  template <typename Filter>
  void PerformTest(std::function<void(Filter const&)> onApply) {
    auto maybe_stream = CreateFilter(filter_, [&] {
      auto mock_impl = std::make_unique<MockStream>();
      EXPECT_CALL(*mock_impl, ApplyFilter)
          .WillOnce([onApply](InternalFilter const& internal_filter) -> bool {
            auto const* maybe_regex = absl::get_if<Filter>(&internal_filter);
            EXPECT_NE(nullptr, maybe_regex);
            onApply(*maybe_regex);
            return true;
          });
      return CellStream(std::move(mock_impl));
    });
    ASSERT_STATUS_OK(maybe_stream);
    // Verify that no separate CellStream object was created when filter is
    // applied internally.
    EXPECT_NE(nullptr, dynamic_cast<MockStream const*>(&maybe_stream->impl()));
  }
};

TEST_F(InternalFiltersAreApplied, RowKeyRegex) {
  filter_.set_row_key_regex_filter("foo.*");

  PerformTest<RowKeyRegex>([](RowKeyRegex const& row_key_regex) {
    EXPECT_EQ("foo.*", row_key_regex.regex->pattern());
  });
}

TEST_F(InternalFiltersAreApplied, FamilyNameRegex) {
  filter_.set_family_name_regex_filter("foo.*");

  PerformTest<FamilyNameRegex>([](FamilyNameRegex const& family_name_regex) {
    EXPECT_EQ("foo.*", family_name_regex.regex->pattern());
  });
}

TEST_F(InternalFiltersAreApplied, ColumnRegex) {
  filter_.set_column_qualifier_regex_filter("foo.*");

  PerformTest<ColumnRegex>([](ColumnRegex const& column_qualifier_regex) {
    EXPECT_EQ("foo.*", column_qualifier_regex.regex->pattern());
  });
}

TEST_F(InternalFiltersAreApplied, ColumnRange) {
  filter_.mutable_column_range_filter()->set_family_name("fam1");
  filter_.mutable_column_range_filter()->set_start_qualifier_open("q1");
  filter_.mutable_column_range_filter()->set_end_qualifier_closed("q4");

  PerformTest<ColumnRange>([](ColumnRange const& column_range) {
    EXPECT_EQ("fam1", column_range.column_family);
    EXPECT_EQ("q1", column_range.range.start());
    EXPECT_TRUE(column_range.range.start_open());
    EXPECT_EQ("q4", column_range.range.end());
    EXPECT_TRUE(column_range.range.end_closed());
  });
}

TEST_F(InternalFiltersAreApplied, TimestampRange) {
  filter_.mutable_timestamp_range_filter()->set_start_timestamp_micros(1000);
  filter_.mutable_timestamp_range_filter()->set_end_timestamp_micros(2000);

  PerformTest<TimestampRange>([](TimestampRange const& timestamp_range) {
    EXPECT_EQ(std::chrono::milliseconds(1), timestamp_range.range.start());
    EXPECT_EQ(std::chrono::milliseconds(2), timestamp_range.range.end());
  });
}

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
