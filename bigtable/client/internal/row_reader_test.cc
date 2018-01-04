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

#include "bigtable/client/internal/row_reader.h"

#include <gmock/gmock.h>
#include <grpc++/test/mock_stream.h>

#include "bigtable/client/testing/table_test_fixture.h"

using testing::_;
using testing::ByMove;
using testing::Return;

using MockResponseStream =
    grpc::testing::MockClientReader<google::bigtable::v2::ReadRowsResponse>;

class RowReaderTest : public bigtable::testing::TableTestFixture {};

TEST_F(RowReaderTest, EmptyReaderHasNoRows) {
  auto stream = new MockResponseStream();  // wrapped in unique_ptr by ReadRows
  EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
  EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, _)).WillOnce(Return(stream));

  bigtable::RowReader reader(client_, "");

  EXPECT_EQ(reader.begin(), reader.end());
}
