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

#include "google/cloud/internal/big_endian.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <ios>
#include <string>
#include <vector>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

template <typename T>
struct TestData {
  T n;
  std::string s;
};

template <typename T>
void RunTests(std::vector<TestData<T>> const& test_data) {
  for (auto const& td : test_data) {
    std::string const encoded = EncodeBigEndian(td.n);
    // The std::hex io manipulator doesn't work right with `char`, so we add 0
    // (an int) to it so that small types are promoted to at least an int.
    auto const for_hex = td.n + 0;
    EXPECT_EQ(td.s, encoded) << "n=" << td.n << " hex=" << std::hex << for_hex;
    auto const decoded = DecodeBigEndian<T>(encoded);
    EXPECT_STATUS_OK(decoded) << "n=" << td.n << " hex=" << std::hex << for_hex;
    EXPECT_EQ(td.n, *decoded);
  }
}

TEST(RoundTripBigEndian, Int8) {
  std::vector<TestData<std::int8_t>> test_data = {
      {-128, std::string("\x80", 1)},  //
      {-127, std::string("\x81", 1)},  //
      {-2, std::string("\xFE", 1)},    //
      {-1, std::string("\xFF", 1)},    //
      {0, std::string("\0", 1)},       //
      {1, std::string("\x01", 1)},     //
      {2, std::string("\x02", 1)},     //
      {127, std::string("\x7F", 1)},   //
  };
  SCOPED_TRACE("std::int8_t");
  RunTests(test_data);
}

TEST(RoundTripBigEndian, UInt8) {
  std::vector<TestData<std::uint8_t>> test_data = {
      {0, std::string("\0", 1)},      //
      {1, std::string("\x01", 1)},    //
      {2, std::string("\x02", 1)},    //
      {127, std::string("\x7F", 1)},  //
      {128, std::string("\x80", 1)},  //
      {255, std::string("\xFF", 1)},  //
  };
  SCOPED_TRACE("std::uint8_t");
  RunTests(test_data);
}

TEST(RoundTripBigEndian, Int16) {
  std::vector<TestData<std::int16_t>> test_data = {
      {-0x7FFF - 1, std::string("\x80\x00", 2)},
      {-257, std::string("\xFE\xFF", 2)},
      {-256, std::string("\xFF\x00", 2)},
      {-255, std::string("\xFF\x01", 2)},
      {-2, std::string("\xFF\xFE", 2)},
      {-1, std::string("\xFF\xFF", 2)},
      {0, std::string("\0\0", 2)},
      {1, std::string("\0\x01", 2)},
      {255, std::string("\0\xFF", 2)},
      {256, std::string("\x01\x00", 2)},
      {0x7F00, std::string("\x7F\x00", 2)},
      {0x7FFF, std::string("\x7F\xFF", 2)},
  };
  SCOPED_TRACE("std::int16_t");
  RunTests(test_data);
}

TEST(RoundTripBigEndian, UInt16) {
  std::vector<TestData<std::uint16_t>> test_data = {
      {0, std::string("\0\0", 2)},
      {1, std::string("\0\x01", 2)},
      {255, std::string("\0\xFF", 2)},
      {256, std::string("\x01\x00", 2)},
      {0x7F00, std::string("\x7F\x00", 2)},
      {0x7FFF, std::string("\x7F\xFF", 2)},
      {0xFFFF, std::string("\xFF\xFF", 2)},
  };
  SCOPED_TRACE("std::uint16_t");
  RunTests(test_data);
}

TEST(RoundTripBigEndian, Int32) {
  std::vector<TestData<std::int32_t>> test_data = {
      {-0x7FFFFFFF - 1, std::string("\x80\0\0\0", 4)},
      {-257, std::string("\xFF\xFF\xFE\xFF", 4)},
      {-256, std::string("\xFF\xFF\xFF\x00", 4)},
      {-255, std::string("\xFF\xFF\xFF\x01", 4)},
      {-2, std::string("\xFF\xFF\xFF\xFE", 4)},
      {-1, std::string("\xFF\xFF\xFF\xFF", 4)},
      {0, std::string("\0\0\0\0", 4)},
      {1, std::string("\0\0\0\x01", 4)},
      {255, std::string("\0\0\0\xFF", 4)},
      {256, std::string("\0\0\x01\x00", 4)},
      {0xFF00, std::string("\0\0\xFF\x00", 4)},
      {0xFFFF, std::string("\0\0\xFF\xFF", 4)},
      {0x7FFFFFFF, std::string("\x7F\xFF\xFF\xFF", 4)},
  };
  SCOPED_TRACE("std::int32_t");
  RunTests(test_data);
}

TEST(RoundTripBigEndian, UInt32) {
  std::vector<TestData<std::uint32_t>> test_data = {
      {0, std::string("\0\0\0\0", 4)},
      {1, std::string("\0\0\0\x01", 4)},
      {255, std::string("\0\0\0\xFF", 4)},
      {256, std::string("\0\0\x01\x00", 4)},
      {0xFF00, std::string("\0\0\xFF\x00", 4)},
      {0xFFFF, std::string("\0\0\xFF\xFF", 4)},
      {0xFFFFFFFF, std::string("\xFF\xFF\xFF\xFF", 4)},
  };
  SCOPED_TRACE("std::uint32_t");
  RunTests(test_data);
}

TEST(RoundTripBigEndian, Int64) {
  std::vector<TestData<std::int64_t>> test_data = {
      {-0x7FFFFFFFFFFFFFFF - 1, std::string("\x80\0\0\0\0\0\0\0", 8)},
      {-257, std::string("\xFF\xFF\xFF\xFF\xFF\xFF\xFE\xFF", 8)},
      {-256, std::string("\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00", 8)},
      {-255, std::string("\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x01", 8)},
      {-2, std::string("\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFE", 8)},
      {-1, std::string("\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 8)},
      {0, std::string("\0\0\0\0\0\0\0\0", 8)},
      {1, std::string("\0\0\0\0\0\0\0\x01", 8)},
      {255, std::string("\0\0\0\0\0\0\0\xFF", 8)},
      {256, std::string("\0\0\0\0\0\0\x01\x00", 8)},
      {0xFF00, std::string("\0\0\0\0\0\0\xFF\x00", 8)},
      {0xFFFF, std::string("\0\0\0\0\0\0\xFF\xFF", 8)},
      {0x7FFFFFFFFFFFFFFF, std::string("\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 8)},
  };
  SCOPED_TRACE("std::int64_t");
  RunTests(test_data);
}

TEST(RoundTripBigEndian, UInt64) {
  std::vector<TestData<std::uint64_t>> test_data = {
      {0, std::string("\0\0\0\0\0\0\0\0", 8)},
      {1, std::string("\0\0\0\0\0\0\0\x01", 8)},
      {255, std::string("\0\0\0\0\0\0\0\xFF", 8)},
      {256, std::string("\0\0\0\0\0\0\x01\x00", 8)},
      {0xFF00, std::string("\0\0\0\0\0\0\xFF\x00", 8)},
      {0xFFFF, std::string("\0\0\0\0\0\0\xFF\xFF", 8)},
      {0xFFFFFFFFFFFFFFFF, std::string("\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 8)},
  };
  SCOPED_TRACE("std::uint64_t");
  RunTests(test_data);
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
