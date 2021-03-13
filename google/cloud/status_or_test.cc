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

#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/expect_exception.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/testing_types.h"
#include <gmock/gmock.h>
#include <type_traits>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace {
using ::testing::HasSubstr;

TEST(StatusOrTest, ValueType) {
  struct Foo {};
  static_assert(std::is_same<StatusOr<int>::value_type, int>::value, "");
  static_assert(std::is_same<StatusOr<char>::value_type, char>::value, "");
  static_assert(std::is_same<StatusOr<Foo>::value_type, Foo>::value, "");

  static_assert(!std::is_same<StatusOr<int>::value_type, char>::value, "");
  static_assert(!std::is_same<StatusOr<char>::value_type, Foo>::value, "");
  static_assert(!std::is_same<StatusOr<Foo>::value_type, int>::value, "");
}

TEST(StatusOrTest, DefaultConstructor) {
  StatusOr<int> actual;
  EXPECT_FALSE(actual.ok());
  EXPECT_FALSE(actual.status().ok());
  EXPECT_FALSE(actual);
}

TEST(StatusOrTest, StatusConstructorNormal) {
  StatusOr<int> actual(Status(StatusCode::kNotFound, "NOT FOUND"));
  EXPECT_FALSE(actual.ok());
  EXPECT_FALSE(actual);
  EXPECT_EQ(StatusCode::kNotFound, actual.status().code());
  EXPECT_EQ("NOT FOUND", actual.status().message());
}

TEST(StatusOrTest, StatusConstructorInvalid) {
  testing_util::ExpectException<std::invalid_argument>(
      [&] { StatusOr<int> actual(Status{}); },
      [&](std::invalid_argument const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("StatusOr"));
      },
      "exceptions are disabled: ");
}

TEST(StatusOrTest, StatusAssignment) {
  auto const error = Status(StatusCode::kUnknown, "blah");
  StatusOr<int> sor;
  sor = error;
  EXPECT_FALSE(sor.ok());
  EXPECT_EQ(error, sor.status());
}

struct NoEquality {};

TEST(StatusOrTest, Equality) {
  auto const err1 = Status(StatusCode::kUnknown, "foo");
  auto const err2 = Status(StatusCode::kUnknown, "bar");

  EXPECT_EQ(StatusOr<int>(err1), StatusOr<int>(err1));
  EXPECT_NE(StatusOr<int>(err1), StatusOr<int>(err2));

  EXPECT_NE(StatusOr<int>(err1), StatusOr<int>(1));
  EXPECT_NE(StatusOr<int>(1), StatusOr<int>(err1));

  EXPECT_EQ(StatusOr<int>(1), StatusOr<int>(1));
  EXPECT_NE(StatusOr<int>(1), StatusOr<int>(2));

  // Verify that we can still construct a StatusOr with a type that does not
  // support equality, we just can't compare the StatusOr for equality with
  // anything else (obviously).
  StatusOr<NoEquality> no_equality;
  no_equality = err1;
  no_equality = NoEquality{};
}

TEST(StatusOrTest, ValueConstructor) {
  StatusOr<int> actual(42);
  EXPECT_STATUS_OK(actual);
  EXPECT_TRUE(actual);
  EXPECT_EQ(42, actual.value());
  EXPECT_EQ(42, std::move(actual).value());
}

TEST(StatusOrTest, ValueConstAccessors) {
  StatusOr<int> const actual(42);
  EXPECT_STATUS_OK(actual);
  EXPECT_EQ(42, actual.value());
  EXPECT_EQ(42, std::move(actual).value());
}

TEST(StatusOrTest, ValueAccessorNonConstThrows) {
  StatusOr<int> actual(Status(StatusCode::kInternal, "BAD"));

  testing_util::ExpectException<RuntimeStatusError>(
      [&] { actual.value(); },
      [&](RuntimeStatusError const& ex) {
        EXPECT_EQ(StatusCode::kInternal, ex.status().code());
        EXPECT_EQ("BAD", ex.status().message());
      },
      "exceptions are disabled: BAD \\[INTERNAL\\]");

  testing_util::ExpectException<RuntimeStatusError>(
      [&] { std::move(actual).value(); },
      [&](RuntimeStatusError const& ex) {
        EXPECT_EQ(StatusCode::kInternal, ex.status().code());
        EXPECT_EQ("BAD", ex.status().message());
      },
      "exceptions are disabled: BAD \\[INTERNAL\\]");
}

TEST(StatusOrTest, ValueAccessorConstThrows) {
  StatusOr<int> actual(Status(StatusCode::kInternal, "BAD"));

  testing_util::ExpectException<RuntimeStatusError>(
      [&] { actual.value(); },
      [&](RuntimeStatusError const& ex) {
        EXPECT_EQ(StatusCode::kInternal, ex.status().code());
        EXPECT_EQ("BAD", ex.status().message());
      },
      "exceptions are disabled: BAD \\[INTERNAL\\]");

  testing_util::ExpectException<RuntimeStatusError>(
      [&] { std::move(actual).value(); },
      [&](RuntimeStatusError const& ex) {
        EXPECT_EQ(StatusCode::kInternal, ex.status().code());
        EXPECT_EQ("BAD", ex.status().message());
      },
      "exceptions are disabled: BAD \\[INTERNAL\\]");
}

TEST(StatusOrTest, StatusConstAccessors) {
  StatusOr<int> const actual(Status(StatusCode::kInternal, "BAD"));
  EXPECT_EQ(StatusCode::kInternal, actual.status().code());
  EXPECT_EQ(StatusCode::kInternal, std::move(actual).status().code());
}

TEST(StatusOrTest, ValueDeference) {
  StatusOr<std::string> actual("42");
  EXPECT_STATUS_OK(actual);
  EXPECT_EQ("42", *actual);
  EXPECT_EQ("42", std::move(actual).value());
}

TEST(StatusOrTest, ValueConstDeference) {
  StatusOr<std::string> const actual("42");
  EXPECT_STATUS_OK(actual);
  EXPECT_EQ("42", *actual);
  EXPECT_EQ("42", std::move(actual).value());
}

TEST(StatusOrTest, ValueArrow) {
  StatusOr<std::string> actual("42");
  EXPECT_STATUS_OK(actual);
  EXPECT_EQ(std::string("42"), actual->c_str());
}

TEST(StatusOrTest, ValueConstArrow) {
  StatusOr<std::string> const actual("42");
  EXPECT_STATUS_OK(actual);
  EXPECT_EQ(std::string("42"), actual->c_str());
}

using testing_util::NoDefaultConstructor;

TEST(StatusOrNoDefaultConstructor, DefaultConstructed) {
  StatusOr<NoDefaultConstructor> empty;
  EXPECT_FALSE(empty.ok());
}

TEST(StatusOrNoDefaultConstructor, ValueConstructed) {
  StatusOr<NoDefaultConstructor> actual(
      NoDefaultConstructor(std::string("foo")));
  EXPECT_STATUS_OK(actual);
  EXPECT_EQ(actual->str(), "foo");
}

using testing_util::Observable;

/// @test A default-constructed status does not call the default constructor.
TEST(StatusOrObservableTest, NoDefaultConstruction) {
  Observable::reset_counters();
  StatusOr<Observable> other;
  EXPECT_EQ(0, Observable::default_constructor());
  EXPECT_FALSE(other.ok());
}

/// @test A copy-constructed status calls the copy constructor for T.
TEST(StatusOrObservableTest, Copy) {
  Observable::reset_counters();
  StatusOr<Observable> other(Observable("foo"));
  EXPECT_EQ("foo", other.value().str());
  EXPECT_EQ(1, Observable::move_constructor());

  Observable::reset_counters();
  StatusOr<Observable> copy(other);
  EXPECT_EQ(1, Observable::copy_constructor());
  EXPECT_STATUS_OK(copy);
  EXPECT_STATUS_OK(other);
  EXPECT_EQ("foo", copy->str());
}

/// @test A move-constructed status calls the move constructor for T.
TEST(StatusOrObservableTest, MoveCopy) {
  Observable::reset_counters();
  StatusOr<Observable> other(Observable("foo"));
  EXPECT_EQ("foo", other.value().str());
  EXPECT_EQ(1, Observable::move_constructor());

  Observable::reset_counters();
  StatusOr<Observable> copy(std::move(other));
  EXPECT_EQ(1, Observable::move_constructor());
  EXPECT_STATUS_OK(copy);
  EXPECT_EQ("foo", copy->str());
  EXPECT_STATUS_OK(other);  // NOLINT(bugprone-use-after-move)
  EXPECT_EQ("moved-out", other->str());
}

/// @test A move-assigned status calls the right assignments and destructors.
TEST(StatusOrObservableTest, MoveAssignmentNoValueNoValue) {
  StatusOr<Observable> other;
  StatusOr<Observable> assigned;
  EXPECT_FALSE(other.ok());
  EXPECT_FALSE(assigned.ok());

  Observable::reset_counters();
  assigned = std::move(other);
  EXPECT_FALSE(other.ok());  // NOLINT(bugprone-use-after-move)
  EXPECT_FALSE(assigned.ok());
  EXPECT_EQ(0, Observable::destructor());
  EXPECT_EQ(0, Observable::move_assignment());
  EXPECT_EQ(0, Observable::copy_assignment());
  EXPECT_EQ(0, Observable::move_constructor());
  EXPECT_EQ(0, Observable::copy_constructor());
}

/// @test A move-assigned status calls the right assignments and destructors.
TEST(StatusOrObservableTest, MoveAssignmentNoValueValue) {
  StatusOr<Observable> other(Observable("foo"));
  StatusOr<Observable> assigned;
  EXPECT_STATUS_OK(other);
  EXPECT_FALSE(assigned.ok());

  Observable::reset_counters();
  assigned = std::move(other);
  EXPECT_STATUS_OK(other);  // NOLINT(bugprone-use-after-move)
  EXPECT_STATUS_OK(assigned);
  EXPECT_EQ("foo", assigned->str());
  EXPECT_EQ("moved-out", other->str());
  EXPECT_EQ(0, Observable::destructor());
  EXPECT_EQ(0, Observable::move_assignment());
  EXPECT_EQ(0, Observable::copy_assignment());
  EXPECT_EQ(1, Observable::move_constructor());
  EXPECT_EQ(0, Observable::copy_constructor());
}

/// @test A move-assigned status calls the right assignments and destructors.
TEST(StatusOrObservableTest, MoveAssignmentNoValueT) {
  Observable other("foo");
  StatusOr<Observable> assigned;
  EXPECT_FALSE(assigned.ok());

  Observable::reset_counters();
  assigned = std::move(other);
  EXPECT_STATUS_OK(assigned);
  EXPECT_EQ("foo", assigned->str());
  EXPECT_EQ("moved-out", other.str());  // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(0, Observable::destructor());
  EXPECT_EQ(0, Observable::move_assignment());
  EXPECT_EQ(0, Observable::copy_assignment());
  EXPECT_EQ(1, Observable::move_constructor());
  EXPECT_EQ(0, Observable::copy_constructor());
}

/// @test A move-assigned status calls the right assignments and destructors.
TEST(StatusOrObservableTest, MoveAssignmentValueNoValue) {
  StatusOr<Observable> other;
  StatusOr<Observable> assigned(Observable("bar"));
  EXPECT_FALSE(other.ok());
  EXPECT_STATUS_OK(assigned);

  Observable::reset_counters();
  assigned = std::move(other);
  EXPECT_FALSE(other.ok());  // NOLINT(bugprone-use-after-move)
  EXPECT_FALSE(assigned.ok());
  EXPECT_EQ(1, Observable::destructor());
  EXPECT_EQ(0, Observable::move_assignment());
  EXPECT_EQ(0, Observable::copy_assignment());
  EXPECT_EQ(0, Observable::move_constructor());
  EXPECT_EQ(0, Observable::copy_constructor());
}

/// @test A move-assigned status calls the right assignments and destructors.
TEST(StatusOrObservableTest, MoveAssignmentValueValue) {
  StatusOr<Observable> other(Observable("foo"));
  StatusOr<Observable> assigned(Observable("bar"));
  EXPECT_STATUS_OK(other);
  EXPECT_STATUS_OK(assigned);

  Observable::reset_counters();
  assigned = std::move(other);
  EXPECT_STATUS_OK(other);  // NOLINT(bugprone-use-after-move)
  EXPECT_STATUS_OK(assigned);
  EXPECT_EQ(0, Observable::destructor());
  EXPECT_EQ(1, Observable::move_assignment());
  EXPECT_EQ(0, Observable::copy_assignment());
  EXPECT_EQ(0, Observable::move_constructor());
  EXPECT_EQ(0, Observable::copy_constructor());
  EXPECT_EQ("foo", assigned->str());
  EXPECT_EQ("moved-out", other->str());
}

/// @test A move-assigned status calls the right assignments and destructors.
TEST(StatusOrObservableTest, MoveAssignmentValueT) {
  Observable other("foo");
  StatusOr<Observable> assigned(Observable("bar"));
  EXPECT_STATUS_OK(assigned);

  Observable::reset_counters();
  assigned = std::move(other);
  EXPECT_STATUS_OK(assigned);
  EXPECT_EQ(0, Observable::destructor());
  EXPECT_EQ(1, Observable::move_assignment());
  EXPECT_EQ(0, Observable::copy_assignment());
  EXPECT_EQ(0, Observable::move_constructor());
  EXPECT_EQ(0, Observable::copy_constructor());
  EXPECT_EQ("foo", assigned->str());
  EXPECT_EQ("moved-out", other.str());  // NOLINT(bugprone-use-after-move)
}

/// @test A copy-assigned status calls the right assignments and destructors.
TEST(StatusOrObservableTest, CopyAssignmentNoValueNoValue) {
  StatusOr<Observable> other;
  StatusOr<Observable> assigned;
  EXPECT_FALSE(other.ok());
  EXPECT_FALSE(assigned.ok());

  Observable::reset_counters();
  assigned = other;
  EXPECT_FALSE(other.ok());
  EXPECT_FALSE(assigned.ok());
  EXPECT_EQ(0, Observable::destructor());
  EXPECT_EQ(0, Observable::move_assignment());
  EXPECT_EQ(0, Observable::copy_assignment());
  EXPECT_EQ(0, Observable::move_constructor());
  EXPECT_EQ(0, Observable::copy_constructor());
}

/// @test A copy-assigned status calls the right assignments and destructors.
TEST(StatusOrObservableTest, CopyAssignmentNoValueValue) {
  StatusOr<Observable> other(Observable("foo"));
  StatusOr<Observable> assigned;
  EXPECT_STATUS_OK(other);
  EXPECT_FALSE(assigned.ok());

  Observable::reset_counters();
  assigned = other;
  EXPECT_STATUS_OK(other);
  EXPECT_STATUS_OK(assigned);
  EXPECT_EQ("foo", assigned->str());
  EXPECT_EQ("foo", other->str());
  EXPECT_EQ(0, Observable::destructor());
  EXPECT_EQ(0, Observable::move_assignment());
  EXPECT_EQ(0, Observable::copy_assignment());
  EXPECT_EQ(0, Observable::move_constructor());
  EXPECT_EQ(1, Observable::copy_constructor());
}

/// @test A copy-assigned status calls the right assignments and destructors.
TEST(StatusOrObservableTest, CopyAssignmentNoValueT) {
  Observable other("foo");
  StatusOr<Observable> assigned;
  EXPECT_FALSE(assigned.ok());

  Observable::reset_counters();
  assigned = other;
  EXPECT_STATUS_OK(assigned);
  EXPECT_EQ("foo", assigned->str());
  EXPECT_EQ("foo", other.str());
  EXPECT_EQ(0, Observable::destructor());
  EXPECT_EQ(0, Observable::move_assignment());
  EXPECT_EQ(0, Observable::copy_assignment());
  EXPECT_EQ(0, Observable::move_constructor());
  EXPECT_EQ(1, Observable::copy_constructor());
}

/// @test A copy-assigned status calls the right assignments and destructors.
TEST(StatusOrObservableTest, CopyAssignmentValueNoValue) {
  StatusOr<Observable> other;
  StatusOr<Observable> assigned(Observable("bar"));
  EXPECT_FALSE(other.ok());
  EXPECT_STATUS_OK(assigned);

  Observable::reset_counters();
  assigned = other;
  EXPECT_FALSE(other.ok());
  EXPECT_FALSE(assigned.ok());
  EXPECT_EQ(1, Observable::destructor());
  EXPECT_EQ(0, Observable::move_assignment());
  EXPECT_EQ(0, Observable::copy_assignment());
  EXPECT_EQ(0, Observable::move_constructor());
  EXPECT_EQ(0, Observable::copy_constructor());
}

/// @test A copy-assigned status calls the right assignments and destructors.
TEST(StatusOrObservableTest, CopyAssignmentValueValue) {
  StatusOr<Observable> other(Observable("foo"));
  StatusOr<Observable> assigned(Observable("bar"));
  EXPECT_STATUS_OK(other);
  EXPECT_STATUS_OK(assigned);

  Observable::reset_counters();
  assigned = other;
  EXPECT_STATUS_OK(other);
  EXPECT_STATUS_OK(assigned);
  EXPECT_EQ(0, Observable::destructor());
  EXPECT_EQ(0, Observable::move_assignment());
  EXPECT_EQ(1, Observable::copy_assignment());
  EXPECT_EQ(0, Observable::move_constructor());
  EXPECT_EQ(0, Observable::copy_constructor());
  EXPECT_EQ("foo", assigned->str());
  EXPECT_EQ("foo", other->str());
}

/// @test A copy-assigned status calls the right assignments and destructors.
TEST(StatusOrObservableTest, CopyAssignmentValueT) {
  Observable other("foo");
  StatusOr<Observable> assigned(Observable("bar"));
  EXPECT_STATUS_OK(assigned);

  Observable::reset_counters();
  assigned = other;
  EXPECT_STATUS_OK(assigned);
  EXPECT_EQ(0, Observable::destructor());
  EXPECT_EQ(0, Observable::move_assignment());
  EXPECT_EQ(1, Observable::copy_assignment());
  EXPECT_EQ(0, Observable::move_constructor());
  EXPECT_EQ(0, Observable::copy_constructor());
  EXPECT_EQ("foo", assigned->str());
  EXPECT_EQ("foo", other.str());
}

TEST(StatusOrObservableTest, MoveValue) {
  StatusOr<Observable> other(Observable("foo"));
  EXPECT_EQ("foo", other.value().str());

  Observable::reset_counters();
  auto observed = std::move(other).value();
  EXPECT_EQ("foo", observed.str());
  EXPECT_STATUS_OK(other);  // NOLINT(bugprone-use-after-move)
  EXPECT_EQ("moved-out", other->str());
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
