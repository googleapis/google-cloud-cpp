// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/bytes.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <cstdint>
#include <deque>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

TEST(Bytes, RoundTrip) {
  char c = std::numeric_limits<char>::min();
  std::string chars(1, c);
  while (c != std::numeric_limits<char>::max()) {
    chars.push_back(++c);
  }

  // Empty string.
  std::string data;
  Bytes bytes(data);
  EXPECT_EQ("", spanner_internal::BytesToBase64(bytes));
  EXPECT_EQ(data, bytes.get<std::string>());

  // All 1-char strings.
  data.resize(1);
  for (auto c : chars) {
    data[0] = c;
    Bytes bytes(data);
    EXPECT_EQ(4, spanner_internal::BytesToBase64(bytes).size());
    EXPECT_EQ(data, bytes.get<std::string>());
  }

  // All 2-char strings.
  data.resize(2);
  for (auto c0 : chars) {
    data[0] = c0;
    for (auto c1 : chars) {
      data[1] = c1;
      Bytes bytes(data);
      EXPECT_EQ(4, spanner_internal::BytesToBase64(bytes).size());
      EXPECT_EQ(data, bytes.get<std::string>());
    }
  }

  // Some 3-char strings.
  data.resize(3);
  for (auto c0 : {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j'}) {
    data[0] = c0;
    for (auto c1 : chars) {
      data[1] = c1;
      for (auto c2 : chars) {
        data[2] = c2;
        Bytes bytes(data);
        EXPECT_EQ(4, spanner_internal::BytesToBase64(bytes).size());
        EXPECT_EQ(data, bytes.get<std::string>());
      }
    }
  }
}

TEST(Bytes, Conversions) {
  std::string const s_coded = "Zm9vYmFy";
  std::string const s_plain = "foobar";
  std::deque<char> const d_plain(s_plain.begin(), s_plain.end());
  std::vector<std::uint8_t> const v_plain(s_plain.begin(), s_plain.end());

  auto bytes = spanner_internal::BytesFromBase64(s_coded);
  EXPECT_STATUS_OK(bytes) << s_coded;
  EXPECT_EQ(s_coded, spanner_internal::BytesToBase64(*bytes));
  EXPECT_EQ(s_plain, bytes->get<std::string>());
  EXPECT_EQ(d_plain, bytes->get<std::deque<char>>());
  EXPECT_EQ(v_plain, bytes->get<std::vector<std::uint8_t>>());

  bytes = Bytes(s_plain);
  EXPECT_EQ(s_coded, spanner_internal::BytesToBase64(*bytes));
  EXPECT_EQ(s_plain, bytes->get<std::string>());
  EXPECT_EQ(d_plain, bytes->get<std::deque<char>>());
  EXPECT_EQ(v_plain, bytes->get<std::vector<std::uint8_t>>());

  bytes = Bytes(d_plain);
  EXPECT_EQ(s_coded, spanner_internal::BytesToBase64(*bytes));
  EXPECT_EQ(s_plain, bytes->get<std::string>());
  EXPECT_EQ(d_plain, bytes->get<std::deque<char>>());
  EXPECT_EQ(v_plain, bytes->get<std::vector<std::uint8_t>>());

  bytes = Bytes(v_plain);
  EXPECT_EQ(s_coded, spanner_internal::BytesToBase64(*bytes));
  EXPECT_EQ(s_plain, bytes->get<std::string>());
  EXPECT_EQ(d_plain, bytes->get<std::deque<char>>());
  EXPECT_EQ(v_plain, bytes->get<std::vector<std::uint8_t>>());
}

TEST(Bytes, RelationalOperators) {
  std::string const s_plain = "The quick brown fox jumps over the lazy dog.";
  std::deque<char> const d_plain(s_plain.begin(), s_plain.end());
  std::vector<std::uint8_t> const v_plain(s_plain.begin(), s_plain.end());

  auto s_bytes = Bytes(s_plain.begin(), s_plain.end());
  auto d_bytes = Bytes(d_plain.begin(), d_plain.end());
  auto v_bytes = Bytes(v_plain.begin(), v_plain.end());
  EXPECT_EQ(s_bytes, d_bytes);
  EXPECT_EQ(d_bytes, v_bytes);
  EXPECT_EQ(v_bytes, s_bytes);

  auto x_bytes = Bytes(s_plain + " How vexingly quick daft zebras jump!");
  EXPECT_NE(x_bytes, s_bytes);
  EXPECT_NE(x_bytes, d_bytes);
  EXPECT_NE(x_bytes, v_bytes);
}

TEST(Bytes, OutputStream) {
  struct TestCase {
    Bytes bytes;
    std::string expected;
  };

  std::vector<TestCase> test_cases = {
      {Bytes(std::string("")), R"(B"")"},
      {Bytes(std::string("foo")), R"(B"foo")"},
      {Bytes(std::string("a\011B")), R"(B"a\011B")"},
      {Bytes(std::string("a\377B")), R"(B"a\377B")"},
      {Bytes(std::string("!@#$%^&*()-.")), R"(B"!@#$%^&*()-.")"},
      {Bytes(std::string(3, '\0')), R"(B"\000\000\000")"},
      {Bytes(""), R"(B"\000")"},
      {Bytes("foo"), R"(B"foo\000")"},
  };

  for (auto const& tc : test_cases) {
    std::ostringstream ss;
    ss << tc.bytes;
    EXPECT_EQ(ss.str(), tc.expected);
  }
}

TEST(Bytes, OutputStreamEscapingCannotFail) {
  using ByteType = unsigned char;
  for (int i = 0; i < std::numeric_limits<ByteType>::max() + 1; ++i) {
    auto const b = static_cast<ByteType>(i);
    std::ostringstream ss;
    ss << Bytes(std::array<ByteType, 1>{b});
    EXPECT_NE(ss.str(), R"(B"\?")") << "i=" << i;
  }
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
