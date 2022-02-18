// Copyright 2020 Google LLC
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

#include "google/cloud/stream_range.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <deque>
#include <vector>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;
using ::testing::UnitTest;

struct StringOption {
  using Type = std::string;
};

std::string CurrentTestName() {
  return UnitTest::GetInstance()->current_test_info()->name();
}

template <typename T>
StreamRange<T> MakeStreamRange(internal::StreamReader<T> reader) {
  internal::OptionsSpan span(Options{}.set<StringOption>(CurrentTestName()));
  return internal::MakeStreamRange(std::move(reader));
}

TEST(StreamRange, DefaultConstructed) {
  StreamRange<int> sr;
  auto it = sr.begin();
  auto end = sr.end();
  EXPECT_EQ(it, end);
  EXPECT_EQ(it, it);
  EXPECT_EQ(end, end);
}

TEST(StreamRange, MoveOnly) {
  auto const reader = [] { return Status{}; };
  auto sr = MakeStreamRange<int>(reader);
  auto move_construct = std::move(sr);
  auto move_assign = MakeStreamRange<int>(reader);
  move_assign = std::move(move_construct);
}

TEST(StreamRange, EmptyRange) {
  auto sr = MakeStreamRange<int>([] { return Status{}; });
  auto it = sr.begin();
  auto end = sr.end();
  EXPECT_EQ(it, end);
  EXPECT_EQ(it, it);
  EXPECT_EQ(end, end);
}

TEST(StreamRange, OneElement) {
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto counter = 0;
  auto reader = [&counter]() -> internal::StreamReader<int>::result_type {
    EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
              CurrentTestName());
    if (counter++ < 1) return 42;
    return Status{};
  };

  auto sr = MakeStreamRange<int>(std::move(reader));
  auto it = sr.begin();
  EXPECT_NE(it, sr.end());
  EXPECT_TRUE(*it);
  EXPECT_EQ(**it, 42);
  ++it;
  EXPECT_EQ(it, sr.end());
}

TEST(StreamRange, OneError) {
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto reader = []() -> internal::StreamReader<int>::result_type {
    EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
              CurrentTestName());
    return Status(StatusCode::kUnknown, "oops");
  };

  auto sr = MakeStreamRange<int>(std::move(reader));
  auto it = sr.begin();
  EXPECT_NE(it, sr.end());
  EXPECT_FALSE(*it);
  EXPECT_THAT(*it, StatusIs(StatusCode::kUnknown, "oops"));
  ++it;
  EXPECT_EQ(it, sr.end());
}

TEST(StreamRange, FiveElements) {
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto counter = 0;
  auto reader = [&counter]() -> internal::StreamReader<int>::result_type {
    EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
              CurrentTestName());
    if (counter++ < 5) return counter;
    return Status{};
  };

  auto sr = MakeStreamRange<int>(std::move(reader));
  std::vector<int> v;
  for (StatusOr<int>& x : sr) {
    EXPECT_TRUE(x);
    v.push_back(*x);
  }
  EXPECT_THAT(v, ElementsAre(1, 2, 3, 4, 5));
}

TEST(StreamRange, PostFixIteration) {
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto counter = 0;
  auto reader = [&counter]() -> internal::StreamReader<int>::result_type {
    EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
              CurrentTestName());
    if (counter++ < 5) return counter;
    return Status{};
  };

  auto sr = MakeStreamRange<int>(std::move(reader));
  std::vector<int> v;
  // NOLINTNEXTLINE(modernize-loop-convert)
  for (auto it = sr.begin(); it != sr.end(); it++) {
    EXPECT_TRUE(*it);
    v.push_back(**it);
  }
  EXPECT_THAT(v, ElementsAre(1, 2, 3, 4, 5));
}

TEST(StreamRange, Distance) {
  internal::OptionsSpan overlay1(Options{}.set<StringOption>("uh-oh"));

  // Empty range
  auto sr = MakeStreamRange<int>([] { return Status{}; });
  EXPECT_EQ(0, std::distance(sr.begin(), sr.end()));

  // Range of one element
  auto counter = 0;
  auto one = MakeStreamRange<int>(
      [&counter]() -> internal::StreamReader<int>::result_type {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  CurrentTestName());
        if (counter++ < 1) return counter;
        return Status{};
      });
  EXPECT_EQ(1, std::distance(one.begin(), one.end()));

  // Range of five elements
  counter = 0;
  auto five = MakeStreamRange<int>(
      [&counter]() -> internal::StreamReader<int>::result_type {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  CurrentTestName());
        if (counter++ < 5) return counter;
        return Status{};
      });
  EXPECT_EQ(5, std::distance(five.begin(), five.end()));
}

TEST(StreamRange, StreamError) {
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto counter = 0;
  auto reader = [&counter]() -> internal::StreamReader<int>::result_type {
    EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
              CurrentTestName());
    if (counter++ < 2) return counter;
    return Status(StatusCode::kUnknown, "oops");
  };

  auto sr = MakeStreamRange<int>(std::move(reader));

  auto it = sr.begin();
  EXPECT_NE(it, sr.end());
  EXPECT_TRUE(*it);
  EXPECT_EQ(**it, 1);

  ++it;
  EXPECT_NE(it, sr.end());
  EXPECT_TRUE(*it);
  EXPECT_EQ(**it, 2);

  // Error, but we return the Status, not end of stream.
  ++it;
  EXPECT_NE(it, sr.end());
  EXPECT_FALSE(*it);
  EXPECT_THAT(*it, StatusIs(StatusCode::kUnknown, "oops"));

  // Since the previous result was an error, we're at the end.
  ++it;
  EXPECT_EQ(it, sr.end());
}

template <typename ResponseType>
class FakeResumableStreamingReadRpc {
 public:
  ~FakeResumableStreamingReadRpc() {
    EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
              CurrentTestName());
    EXPECT_TRUE(read_called_);
  }

  typename internal::StreamReader<ResponseType>::result_type Read() {
    EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
              CurrentTestName());
    EXPECT_FALSE(read_called_);
    read_called_ = true;
    return ResponseType{};
  }

 private:
  bool read_called_ = false;
};

TEST(StreamRange, ReaderDestructorOptionsSpan) {
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto reader = [] {
    auto resumable = std::make_shared<FakeResumableStreamingReadRpc<int>>();
    return [resumable]() -> internal::StreamReader<int>::result_type {
      return resumable->Read();
    };
  }();
  auto sr = MakeStreamRange<int>(std::move(reader));
  // `~StreamRange()` will now delete the reader, which will drop the last
  // reference to the `FakeResumableStreamingReadRpc`, and all of that should
  // happen with `CurrentOptions()` matching those at `StreamRange` ctor time.
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
