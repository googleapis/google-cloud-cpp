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

#include "google/cloud/bigtable/data_connection.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/emulator/filter.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {
namespace {

using ::testing::Return;
using testing_util::chrono_literals::operator""_ms;

class MockStream : public AbstractCellStreamImpl {
 public:
  MOCK_METHOD(bool, ApplyFilter, (InternalFilter const& internal_filter),
              (override));
  MOCK_METHOD(bool, HasValue, (), (const override));
  MOCK_METHOD(CellView const&, Value, (), (const override));
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
  TestCell(TestCell&& other)
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
  auto& cell_view = test_cell.AsCellView();
  stream << "Cell(" << cell_view.row_key() << " " << cell_view.column_family()
         << ":" << cell_view.column_qualifier() << " @"
         << cell_view.timestamp().count() << "ms: " << cell_view.value() << ")";
  return stream;
}

TEST(CellStream, NextColumnNotSupportedNoMoreData) {
  std::vector<TestCell> cells{
      TestCell{"row1", "cf1", "col1", 0_ms, "val1"}};
  std::vector<TestCell>::iterator cur_cell = cells.begin();

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
  std::vector<TestCell>::iterator cur_cell = cells.begin();

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
  std::vector<TestCell>::iterator cur_cell = cells.begin();

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
  std::vector<TestCell>::iterator cur_cell = cells.begin();

  auto mock_impl = std::make_unique<MockStream>();
  EXPECT_CALL(*mock_impl, Next(NextMode::kRow))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_impl, Value).WillRepeatedly([&]() -> CellView const& {
    return cur_cell->AsCellView();
  });
  EXPECT_CALL(*mock_impl, HasValue).WillRepeatedly([&] {
    return cur_cell != cells.end();
  });
  EXPECT_CALL(*mock_impl, Next(NextMode::kColumn)).WillRepeatedly([&] {
    cur_cell =
        std::find_if(cur_cell, cells.end(), [&](TestCell const& cell) {
          return cell.AsCellView().row_key() !=
                     cur_cell->AsCellView().row_key() ||
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
  std::vector<TestCell>::iterator cur_cell = cells.begin();

  auto mock_impl = std::make_unique<MockStream>();
  EXPECT_CALL(*mock_impl, Next(NextMode::kRow))
      .WillRepeatedly(Return(false));
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
  EXPECT_CALL(*mock_impl_left, Value)
      .WillRepeatedly(
          [&]() -> CellView const& { return left_cell.AsCellView(); });
  EXPECT_CALL(*mock_impl_left, HasValue).WillRepeatedly([&] {
    return true;
  });

  auto mock_impl_right = std::make_unique<MockStream>();
  EXPECT_CALL(*mock_impl_right, Value)
      .WillRepeatedly(
          [&]() -> CellView const& { return right_cell.AsCellView(); });
  EXPECT_CALL(*mock_impl_right, HasValue).WillRepeatedly([&] {
    return true;
  });
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

  std::vector<TestCell>::iterator cur_cell = cells.begin();

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
  TestStreamData(std::vector<TestCell> data)
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

  EXPECT_CALL(*stream_data_1.stream, Next(NextMode::kCell))
      .WillOnce([&]() {
        ++stream_data_1.cur_cell;
        return true;
      });

  EXPECT_CALL(*stream_data_2.stream, Next(NextMode::kCell))
      .Times(2)
      .WillRepeatedly([&]() {
        ++stream_data_2.cur_cell;
        return true;
      });

  EXPECT_CALL(*stream_data_3.stream, Next(NextMode::kCell))
      .WillOnce([&]() {
        ++stream_data_3.cur_cell;
        return true;
      });

  std::vector<CellStream> streams;
  streams.push_back(CellStream(std::move(stream_data_1.stream)));
  streams.push_back(CellStream(std::move(stream_data_2.stream)));
  streams.push_back(CellStream(std::move(stream_data_3.stream)));
  streams.push_back(CellStream(std::move(stream_data_4.stream)));
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

  EXPECT_CALL(*stream_data_different_row.stream,
              Next(NextMode::kCell))
      .WillOnce([&]() {
        ++stream_data_different_row.cur_cell;
        return true;
      });

  std::vector<CellStream> streams;
  streams.push_back(CellStream(std::move(stream_data.stream)));
  streams.push_back(
      CellStream(std::move(stream_data_different_column_family.stream)));
  streams.push_back(
      CellStream(std::move(stream_data_different_column_qualifier.stream)));
  streams.push_back(CellStream(std::move(stream_data_different_row.stream)));
  streams.push_back(CellStream(
      std::move(stream_data_same_column_different_timestamp.stream)));
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

}  // anonymous namespace
}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
