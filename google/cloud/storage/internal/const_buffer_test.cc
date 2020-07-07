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

#include "google/cloud/storage/internal/const_buffer.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::testing::ElementsAre;

TEST(ConstBufferTest, TotalBytesEmpty) {
  ConstBufferSequence const actual;
  EXPECT_EQ(0, TotalBytes(actual));
}

TEST(ConstBufferTest, TotalBytes) {
  ConstBufferSequence const actual{
      ConstBuffer{"1", 1},
      ConstBuffer{"12", 2},
      ConstBuffer{"123", 3},
      ConstBuffer{"", 0},
  };
  EXPECT_EQ(6, TotalBytes(actual));
}

TEST(ConstBufferTest, PopFrontAll) {
  ConstBufferSequence actual{
      ConstBuffer{"1", 1},
      ConstBuffer{"ab", 2},
      ConstBuffer{"ABC", 3},
  };
  PopFrontBytes(actual, 8);
  EXPECT_TRUE(actual.empty());
}

TEST(ConstBufferTest, PopFrontOne) {
  ConstBufferSequence actual{
      ConstBuffer{"1", 1},
      ConstBuffer{"ab", 2},
      ConstBuffer{"ABC", 3},
  };
  PopFrontBytes(actual, 1);
  EXPECT_THAT(actual, ElementsAre(ConstBuffer{"ab", 2}, ConstBuffer{"ABC", 3}));
}

TEST(ConstBufferTest, PopFrontOnePartial) {
  ConstBufferSequence actual{
      ConstBuffer{"abcd", 4},
      ConstBuffer{"ABC", 3},
  };
  PopFrontBytes(actual, 2);
  EXPECT_THAT(actual, ElementsAre(ConstBuffer{"cd", 2}, ConstBuffer{"ABC", 3}));
}

TEST(ConstBufferTest, PopFrontPartial) {
  ConstBufferSequence actual{
      ConstBuffer{"abcd", 4},
      ConstBuffer{"ABC", 3},
      ConstBuffer{"123", 3},
  };
  PopFrontBytes(actual, 6);
  EXPECT_THAT(actual, ElementsAre(ConstBuffer{"C", 1}, ConstBuffer{"123", 3}));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
