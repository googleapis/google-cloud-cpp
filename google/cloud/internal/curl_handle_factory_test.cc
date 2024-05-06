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

#include "google/cloud/internal/curl_handle_factory.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/curl_options.h"
#include <gmock/gmock.h>
#include <map>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

// Version of DefaultCurlHandleFactory that keeps track of what calls have been
// made to SetCurlStringOption.
class OverriddenDefaultCurlHandleFactory : public DefaultCurlHandleFactory {
 public:
  OverriddenDefaultCurlHandleFactory() = default;
  explicit OverriddenDefaultCurlHandleFactory(Options const& options)
      : DefaultCurlHandleFactory(options) {}

 protected:
  void SetCurlStringOption(CURL* handle, CURLoption option_tag,
                           char const* value) override {
    set_options_[option_tag] = value;
    DefaultCurlHandleFactory::SetCurlStringOption(handle, option_tag, value);
  }

 public:
  std::map<int, std::string> set_options_;
};

// Version of DefaultCurlHandleFactory that keeps track of what calls have been
// made to SetCurlStringOption.
class OverriddenPooledCurlHandleFactory : public PooledCurlHandleFactory {
 public:
  explicit OverriddenPooledCurlHandleFactory(std::size_t maximum_size)
      : PooledCurlHandleFactory(maximum_size) {}
  OverriddenPooledCurlHandleFactory(std::size_t maximum_size,
                                    Options const& options)
      : PooledCurlHandleFactory(maximum_size, options) {}

 protected:
  void SetCurlStringOption(CURL* handle, CURLoption option_tag,
                           char const* value) override {
    set_options_[option_tag] = value;
    PooledCurlHandleFactory::SetCurlStringOption(handle, option_tag, value);
  }

 public:
  std::map<int, std::string> set_options_;
};

TEST(CurlHandleFactoryTest,
     DefaultFactoryNoChannelOptionsDoesntCallSetOptions) {
  OverriddenDefaultCurlHandleFactory object_under_test;

  object_under_test.CreateHandle();
  EXPECT_THAT(object_under_test.set_options_, IsEmpty());
}

TEST(CurlHandleFactoryTest, DefaultFactoryChannelOptionsCallsSetOptions) {
  auto options = Options{}.set<CARootsFilePathOption>("foo");
  OverriddenDefaultCurlHandleFactory object_under_test(options);

  auto const expected = std::make_pair(CURLOPT_CAINFO, std::string("foo"));

  object_under_test.CreateHandle();
  EXPECT_THAT(object_under_test.set_options_, ElementsAre(expected));
}

TEST(CurlHandleFactoryTest, PooledFactoryNoChannelOptionsDoesntCallSetOptions) {
  OverriddenPooledCurlHandleFactory object_under_test(2);

  object_under_test.CreateHandle();
  EXPECT_THAT(object_under_test.set_options_, IsEmpty());
}

TEST(CurlHandleFactoryTest, PooledFactoryChannelOptionsCallsSetOptions) {
  auto options = Options{}.set<CARootsFilePathOption>("foo");
  OverriddenPooledCurlHandleFactory object_under_test(2, options);

  auto const expected = std::make_pair(CURLOPT_CAINFO, std::string("foo"));

  {
    object_under_test.CreateHandle();
    EXPECT_THAT(object_under_test.set_options_, ElementsAre(expected));
  }
  // the above should have left the handle in the cache. Check that cached
  // handles get their options set again.
  object_under_test.set_options_.clear();

  object_under_test.CreateHandle();
  EXPECT_THAT(object_under_test.set_options_, ElementsAre(expected));
}

TEST(CurlHandleFactoryTest, Bug14132Regression) {
  auto constexpr kPoolSize = 32;
  PooledCurlHandleFactory pool(kPoolSize);
  std::vector<CurlPtr> handles;
  std::generate_n(std::back_inserter(handles), 2 * kPoolSize,
                  [&]() { return pool.CreateHandle(); });

  for (int i = 4 * kPoolSize; i != 0; --i) {
    std::vector<CurlMulti> multi;
    std::generate_n(std::back_inserter(multi), i,
                    [&]() { return pool.CreateMultiHandle(); });
    for (auto& m : multi) {
      pool.CleanupMultiHandle(std::move(m), HandleDisposition::kKeep);
    }
  }
  for (auto& h : handles) {
    pool.CleanupHandle(std::move(h), HandleDisposition::kDiscard);
  }
}

/// @test An analog to #14132 but with CurlMulti and CurlPtr roles reversed.
TEST(CurlHandleFactoryTest, AnalogToBug14132Regression) {
  auto constexpr kPoolSize = 32;
  PooledCurlHandleFactory pool(kPoolSize);
  std::vector<CurlMulti> multi;
  std::generate_n(std::back_inserter(multi), 2 * kPoolSize,
                  [&]() { return pool.CreateMultiHandle(); });

  for (int i = 4 * kPoolSize; i != 0; --i) {
    std::vector<CurlPtr> handles;
    std::generate_n(std::back_inserter(handles), i,
                    [&]() { return pool.CreateHandle(); });
    for (auto& h : handles) {
      pool.CleanupHandle(std::move(h), HandleDisposition::kKeep);
    }
  }
  for (auto& m : multi) {
    pool.CleanupMultiHandle(std::move(m), HandleDisposition::kDiscard);
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
