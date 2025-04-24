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

#include "google/cloud/internal/pagination_range.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <algorithm>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;
using ::testing::HasSubstr;

struct StringOption {
  using Type = std::string;
};

struct Item {
  std::string data;
};

// A generic request. Fields with a "testonly_" prefix are used for testing but
// are not used in the real code.
struct Request {
  std::string testonly_page_token;
  void set_page_token(std::string token) {
    testonly_page_token = std::move(token);
  }
};

// Looks like a minimal protobuf response message. Fields with a "testonly_"
// prefix are used for testing but are not used in the real code.
struct ProtoResponse {
  std::vector<Item> testonly_items;
  std::string testonly_page_token;
  std::string* mutable_next_page_token() { return &testonly_page_token; }
  std::string const& next_page_token() const { return testonly_page_token; }

  // Used for setting the token in tests, but it's not used in the real code.
  void testonly_set_page_token(std::string s) {
    testonly_page_token = std::move(s);
  }
};

// Looks like a minimal struct response message. Fields with a "testonly_"
// prefix are used for testing but are not used in the real code.
struct StructResponse {
  std::vector<Item> testonly_items;
  std::string next_page_token;

  // Used for setting the token in tests, but it's not used in the real code.
  void testonly_set_page_token(std::string s) {
    next_page_token = std::move(s);
  }
};

using ItemRange = PaginationRange<Item>;

template <typename Response>
class MockRpcImplicit {
 public:
  MOCK_METHOD(StatusOr<Response>, Loader, (Request const&));
};

template <typename Response>
class MockRpcExplicit {
 public:
  MOCK_METHOD(StatusOr<Response>, Loader, (Options const&, Request const&));
};

// A fixture for a "typed test". Each test below will be tested with a
// `ProtoResponse` object and a `StructResponse` object.
template <typename T>
class PaginationRangeTest : public testing::Test {};
using ResponseTypes = ::testing::Types<ProtoResponse, StructResponse>;
TYPED_TEST_SUITE(PaginationRangeTest, ResponseTypes, );

TYPED_TEST(PaginationRangeTest, SinglePageImplicit) {
  using ResponseType = TypeParam;
  MockRpcImplicit<ResponseType> mock;
  EXPECT_CALL(mock, Loader).WillOnce([](Request const& request) {
    EXPECT_EQ(CurrentOptions().get<StringOption>(), "SinglePage");
    EXPECT_TRUE(request.testonly_page_token.empty());
    ResponseType response;
    response.testonly_items.push_back(Item{"p1"});
    response.testonly_items.push_back(Item{"p2"});
    return response;
  });

  OptionsSpan span(Options{}.set<StringOption>("SinglePage"));
  auto range = MakePaginationRange<ItemRange>(
      Request{}, [&mock](Request const& r) { return mock.Loader(r); },
      [](ResponseType const& r) { return r.testonly_items; });
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  std::vector<std::string> names;
  for (auto& p : range) {
    if (!p) break;
    names.push_back(p->data);
  }
  EXPECT_THAT(names, ElementsAre("p1", "p2"));
}

TYPED_TEST(PaginationRangeTest, TypedEmpty) {
  using ResponseType = TypeParam;
  MockRpcExplicit<ResponseType> mock;
  EXPECT_CALL(mock, Loader)
      .WillOnce([](Options const& options, Request const& request) {
        EXPECT_EQ(options.get<StringOption>(), "TypedEmpty");
        EXPECT_TRUE(request.testonly_page_token.empty());
        return ResponseType{};
      });
  auto range = MakePaginationRange<ItemRange>(
      MakeImmutableOptions(Options{}.set<StringOption>("TypedEmpty")),
      Request{},
      [&mock](Options const& o, Request const& r) { return mock.Loader(o, r); },
      [](ResponseType const& r) { return r.testonly_items; });
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  EXPECT_TRUE(range.begin() == range.end());
}

TYPED_TEST(PaginationRangeTest, SinglePage) {
  using ResponseType = TypeParam;
  MockRpcExplicit<ResponseType> mock;
  EXPECT_CALL(mock, Loader)
      .WillOnce([](Options const& options, Request const& request) {
        EXPECT_EQ(options.get<StringOption>(), "SinglePage");
        EXPECT_TRUE(request.testonly_page_token.empty());
        ResponseType response;
        response.testonly_items.push_back(Item{"p1"});
        response.testonly_items.push_back(Item{"p2"});
        return response;
      });

  auto range = MakePaginationRange<ItemRange>(
      MakeImmutableOptions(Options{}.set<StringOption>("SinglePage")),
      Request{},
      [&mock](Options const& o, Request const& r) { return mock.Loader(o, r); },
      [](ResponseType const& r) { return r.testonly_items; });
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  std::vector<std::string> names;
  for (auto& p : range) {
    if (!p) break;
    names.push_back(p->data);
  }
  EXPECT_THAT(names, ElementsAre("p1", "p2"));
}

TYPED_TEST(PaginationRangeTest, NonProtoRange) {
  using ResponseType = TypeParam;
  MockRpcExplicit<ResponseType> mock;
  EXPECT_CALL(mock, Loader)
      .WillOnce([](Options const& options, Request const& request) {
        EXPECT_EQ(options.get<StringOption>(), "NonProtoRange");
        EXPECT_TRUE(request.testonly_page_token.empty());
        ResponseType response;
        response.testonly_items.push_back(Item{"p1"});
        response.testonly_items.push_back(Item{"p2"});
        return response;
      });

  using NonProtoRange = PaginationRange<std::string>;
  auto range = MakePaginationRange<NonProtoRange>(
      MakeImmutableOptions(Options{}.set<StringOption>("NonProtoRange")),
      Request{},
      [&mock](Options const& o, Request const& r) { return mock.Loader(o, r); },
      [](ResponseType const& r) {
        std::vector<std::string> v(r.testonly_items.size());
        std::transform(r.testonly_items.begin(), r.testonly_items.end(),
                       v.begin(),
                       [](auto& item) { return std::move(item.data); });
        return v;
      });
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));

  std::vector<std::string> names;
  for (auto& p : range) {
    if (!p) break;
    names.push_back(*p);
  }
  EXPECT_THAT(names, ElementsAre("p1", "p2"));
}

TYPED_TEST(PaginationRangeTest, TwoPages) {
  using ResponseType = TypeParam;
  MockRpcExplicit<ResponseType> mock;
  EXPECT_CALL(mock, Loader)
      .WillOnce([](Options const& options, Request const& request) {
        EXPECT_EQ(options.get<StringOption>(), "TwoPages");
        EXPECT_TRUE(request.testonly_page_token.empty());
        ResponseType response;
        response.testonly_set_page_token("t1");
        response.testonly_items.push_back(Item{"p1"});
        response.testonly_items.push_back(Item{"p2"});
        return response;
      })
      .WillOnce([](Options const& options, Request const& request) {
        EXPECT_EQ(options.get<StringOption>(), "TwoPages");
        EXPECT_EQ("t1", request.testonly_page_token);
        ResponseType response;
        response.testonly_items.push_back(Item{"p3"});
        response.testonly_items.push_back(Item{"p4"});
        return response;
      });

  auto range = MakePaginationRange<ItemRange>(
      MakeImmutableOptions(Options{}.set<StringOption>("TwoPages")), Request{},
      [&mock](Options const& o, Request const& r) { return mock.Loader(o, r); },
      [](ResponseType const& r) { return r.testonly_items; });
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  std::vector<std::string> names;
  for (auto& p : range) {
    if (!p) break;
    names.push_back(p->data);
  }
  EXPECT_THAT(names, ElementsAre("p1", "p2", "p3", "p4"));
}

TYPED_TEST(PaginationRangeTest, EmptyAndFullPages) {
  using ResponseType = TypeParam;
  MockRpcExplicit<ResponseType> mock;
  EXPECT_CALL(mock, Loader)
      .WillOnce([](Options const& options, Request const& request) {
        EXPECT_EQ(options.get<StringOption>(), "EmptyAndFullPages");
        EXPECT_TRUE(request.testonly_page_token.empty());
        ResponseType response;
        response.testonly_set_page_token("t1");
        return response;
      })
      .WillOnce([](Options const& options, Request const& request) {
        EXPECT_EQ(options.get<StringOption>(), "EmptyAndFullPages");
        EXPECT_EQ("t1", request.testonly_page_token);
        ResponseType response;
        response.testonly_set_page_token("t2");
        return response;
      })
      .WillOnce([](Options const& options, Request const& request) {
        EXPECT_EQ(options.get<StringOption>(), "EmptyAndFullPages");
        EXPECT_EQ("t2", request.testonly_page_token);
        ResponseType response;
        response.testonly_items.push_back(Item{"p1"});
        response.testonly_items.push_back(Item{"p2"});
        response.testonly_set_page_token("t3");
        return response;
      })
      .WillOnce([](Options const& options, Request const& request) {
        EXPECT_EQ(options.get<StringOption>(), "EmptyAndFullPages");
        EXPECT_EQ("t3", request.testonly_page_token);
        ResponseType response;
        response.testonly_set_page_token("t4");
        return response;
      })
      .WillOnce([](Options const& options, Request const& request) {
        EXPECT_EQ(options.get<StringOption>(), "EmptyAndFullPages");
        EXPECT_EQ("t4", request.testonly_page_token);
        ResponseType response;
        response.testonly_items.push_back(Item{"p3"});
        response.testonly_items.push_back(Item{"p4"});
        response.testonly_set_page_token("");
        return response;
      });

  auto range = MakePaginationRange<ItemRange>(
      MakeImmutableOptions(Options{}.set<StringOption>("EmptyAndFullPages")),
      Request{},
      [&mock](Options const& o, Request const& r) { return mock.Loader(o, r); },
      [](ResponseType const& r) { return r.testonly_items; });
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  std::vector<std::string> names;
  for (auto& p : range) {
    if (!p) break;
    names.push_back(p->data);
  }
  EXPECT_THAT(names, ElementsAre("p1", "p2", "p3", "p4"));
}

TYPED_TEST(PaginationRangeTest, TwoPagesWithError) {
  using ResponseType = TypeParam;
  MockRpcExplicit<ResponseType> mock;
  EXPECT_CALL(mock, Loader)
      .WillOnce([](Options const& options, Request const& request) {
        EXPECT_EQ(options.get<StringOption>(), "TwoPagesWithError");
        EXPECT_TRUE(request.testonly_page_token.empty());
        ResponseType response;
        response.testonly_set_page_token("t1");
        response.testonly_items.push_back(Item{"p1"});
        response.testonly_items.push_back(Item{"p2"});
        return response;
      })
      .WillOnce([](Options const& options, Request const& request) {
        EXPECT_EQ(options.get<StringOption>(), "TwoPagesWithError");
        EXPECT_EQ("t1", request.testonly_page_token);
        ResponseType response;
        response.testonly_set_page_token("t2");
        response.testonly_items.push_back(Item{"p3"});
        response.testonly_items.push_back(Item{"p4"});
        return response;
      })
      .WillOnce([](Options const& options, Request const& request) {
        EXPECT_EQ(options.get<StringOption>(), "TwoPagesWithError");
        EXPECT_EQ("t2", request.testonly_page_token);
        return Status(StatusCode::kAborted, "bad-luck");
      });

  auto range = MakePaginationRange<ItemRange>(
      MakeImmutableOptions(Options{}.set<StringOption>("TwoPagesWithError")),
      Request{},
      [&mock](Options const& o, Request const& r) { return mock.Loader(o, r); },
      [](ResponseType const& r) { return r.testonly_items; });
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  std::vector<std::string> names;
  for (auto& p : range) {
    if (!p) {
      EXPECT_EQ(StatusCode::kAborted, p.status().code());
      EXPECT_THAT(p.status().message(), HasSubstr("bad-luck"));
      break;
    }
    names.push_back(p->data);
  }
  EXPECT_THAT(names, ElementsAre("p1", "p2", "p3", "p4"));
}

TYPED_TEST(PaginationRangeTest, IteratorCoverage) {
  using ResponseType = TypeParam;
  MockRpcExplicit<ResponseType> mock;
  EXPECT_CALL(mock, Loader)
      .WillOnce([](Options const& options, Request const& request) {
        EXPECT_EQ(options.get<StringOption>(), "IteratorCoverage");
        EXPECT_TRUE(request.testonly_page_token.empty());
        ResponseType response;
        response.testonly_set_page_token("t1");
        response.testonly_items.push_back(Item{"p1"});
        return response;
      })
      .WillOnce([](Options const& options, Request const& request) {
        EXPECT_EQ(options.get<StringOption>(), "IteratorCoverage");
        EXPECT_EQ("t1", request.testonly_page_token);
        return Status(StatusCode::kAborted, "bad-luck");
      });

  auto range = MakePaginationRange<ItemRange>(
      MakeImmutableOptions(Options{}.set<StringOption>("IteratorCoverage")),
      Request{},
      [&mock](Options const& o, Request const& r) { return mock.Loader(o, r); },
      [](ResponseType const& r) { return r.testonly_items; });
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto i0 = range.begin();
  auto i1 = i0;
  EXPECT_TRUE(i0 == i1);
  EXPECT_FALSE(i1 == range.end());
  ++i1;
  auto i2 = i1;
  EXPECT_TRUE(i1 == i2);
  ASSERT_FALSE(i1 == range.end());
  auto& item = *i1;
  EXPECT_EQ(StatusCode::kAborted, item.status().code());
  EXPECT_THAT(item.status().message(), HasSubstr("bad-luck"));
  ++i1;
  EXPECT_TRUE(i1 == range.end());
}

TEST(RangeFromPagination, MakeUnimplemented) {
  using NonProtoRange = PaginationRange<std::string>;
  auto range = MakeUnimplementedPaginationRange<NonProtoRange>();
  auto i = range.begin();
  EXPECT_NE(i, range.end());
  EXPECT_THAT(*i, StatusIs(StatusCode::kUnimplemented));
}

TEST(RangeFromPagination, MakeStatus) {
  using NonProtoRange = PaginationRange<std::string>;
  auto const expected = InvalidArgumentError("bad", GCP_ERROR_INFO());
  auto range = MakeErrorPaginationRange<NonProtoRange>(expected);
  auto i = range.begin();
  EXPECT_NE(i, range.end());
  EXPECT_EQ(i->status(), expected);
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
