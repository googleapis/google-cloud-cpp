//
// Copyright 2022 Google LLC
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

#include "google/cloud/future.h"
#if GOOGLE_CLOUD_CPP_HAVE_COROUTINES
#include <gmock/gmock.h>
#include <algorithm>
#include <ranges>
#include <string>
#include <thread>
#include <vector>

namespace google::cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::HasSubstr;

future<std::string> AsyncAppend(future<std::string> a, future<int> b) {
  co_return co_await std::move(a) + std::to_string(co_await std::move(b));
}

TEST(FutureCoroutines, BaseGeneric) {
  promise<std::string> pa;
  promise<int> pb;
  auto pending = AsyncAppend(pa.get_future(), pb.get_future());
  pa.set_value("4");
  pb.set_value(2);
  EXPECT_EQ(pending.get(), "42");
}

future<void> WaitAll(std::vector<future<void>> futures) {
  for (auto& f : futures) co_await std::move(f);
}

TEST(FutureCoroutines, BaseVoid) {
  std::vector<promise<void>> promises(3);
  std::vector<future<void>> futures(promises.size());
  std::transform(promises.begin(), promises.end(), futures.begin(),
                 [](auto& p) { return p.get_future(); });
  auto done = WaitAll(std::move(futures));
  for (auto& p : promises | std::views::reverse) p.set_value();

  done.get();
}

template <typename U>
future<std::string> WithCoReturn(future<void> wait, future<U> r) {
  co_await std::move(wait);
  co_return co_await std::move(r);
}

TEST(FutureCoroutines, CoReturnGeneric) {
  promise<std::string> pa;
  promise<void> wait;
  auto x = WithCoReturn(wait.get_future(), pa.get_future());
  EXPECT_FALSE(x.is_ready());
  wait.set_value();
  EXPECT_FALSE(x.is_ready());
  pa.set_value("42");
  EXPECT_TRUE(x.is_ready());
  EXPECT_EQ(x.get(), "42");
}

TEST(FutureCoroutines, CoReturnChange) {
  auto constexpr kValue = "42";
  promise<char const*> pa;
  promise<void> wait;
  auto x = WithCoReturn(wait.get_future(), pa.get_future());
  EXPECT_FALSE(x.is_ready());
  wait.set_value();
  EXPECT_FALSE(x.is_ready());
  pa.set_value(kValue);
  EXPECT_TRUE(x.is_ready());
  x.get();
}

future<std::vector<std::thread::id>> GetThreads(
    std::vector<future<void>> futures) {
  std::vector<std::thread::id> threads(futures.size());
  for (auto& f : futures) {
    co_await std::move(f);
    threads.push_back(std::this_thread::get_id());
  }
  co_return threads;
}

TEST(FutureCoroutines, Threads) {
  std::vector<promise<void>> promises(32);
  std::vector<future<void>> futures(promises.size());
  std::transform(promises.begin(), promises.end(), futures.begin(),
                 [](auto& p) { return p.get_future(); });
  auto done = GetThreads(std::move(futures));

  std::vector<std::jthread> threads(promises.size());
  std::transform(
      promises.begin(), promises.end(), threads.begin(), [](promise<void>& p) {
        return std::jthread{[](auto p) { p.set_value(); }, std::move(p)};
      });
  auto all = done.get();
  std::vector<std::thread::id> ids;
  std::unique_copy(all.begin(), all.end(), std::back_inserter(ids));
  EXPECT_GE(ids.size(), 2);
}

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
future<void> TestThrowVoid(future<void> t) { co_return co_await std::move(t); }

future<int> TestThrowInt(future<int> t) { co_return co_await std::move(t); }

TEST(FutureCoroutines, ThrowVoid) {
  promise<void> p;
  auto f = TestThrowVoid(p.get_future());
  p.set_exception(std::make_exception_ptr(std::runtime_error("test message")));
  EXPECT_THROW(
      try { f.get(); } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("test message"));
        throw;
      },
      std::runtime_error);
}

TEST(FutureCoroutines, ThrowInt) {
  promise<int> p;
  auto f = TestThrowInt(p.get_future());
  p.set_exception(std::make_exception_ptr(std::runtime_error("test message")));
  EXPECT_THROW(
      try { f.get(); } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("test message"));
        throw;
      },
      std::runtime_error);
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace google::cloud

#endif  // GOOGLE_CLOUD_CPP_CPP_HAVE_COROUTINES
