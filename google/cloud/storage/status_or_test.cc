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

#include "google/cloud/storage/status_or.h"
#include "google/cloud/testing_util/expect_exception.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
using ::testing::HasSubstr;

TEST(StatusOrTest, DefaultConstructor) {
  StatusOr<int> actual;
  EXPECT_FALSE(actual.ok());
  EXPECT_FALSE(actual.status().ok());
  EXPECT_TRUE(not actual);
}

TEST(StatusOrTest, StatusConstructorNormal) {
  StatusOr<int> actual(Status(404, "NOT FOUND", "It was there yesterday!"));
  EXPECT_FALSE(actual.ok());
  EXPECT_TRUE(not actual);
  EXPECT_EQ(404, actual.status().status_code());
  EXPECT_EQ("NOT FOUND", actual.status().error_message());
  EXPECT_EQ("It was there yesterday!", actual.status().error_details());
}

TEST(StatusOrTest, StatusConstructorInvalid) {
  testing_util::ExpectException<std::invalid_argument>(
      [&] { StatusOr<int> actual(Status{}); },
      [&](std::invalid_argument const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("StatusOr"));
      },
      "exceptions are disabled: "
      );
}

TEST(StatusOrTest, ValueConstructor) {
  StatusOr<int> actual(42);
  EXPECT_TRUE(actual.ok());
  EXPECT_FALSE(not actual);
  EXPECT_EQ(42, actual.value());
  EXPECT_EQ(42, std::move(actual).value());
}

TEST(StatusOrTest, ValueConstAccessors) {
  StatusOr<int> const actual(42);
  EXPECT_TRUE(actual.ok());
  EXPECT_EQ(42, actual.value());
  EXPECT_EQ(42, std::move(actual).value());
}

TEST(StatusOrTest, ValueAccessorNonConstThrows) {
  StatusOr<int> actual(Status(500, "BAD"));

  testing_util::ExpectException<RuntimeStatusError>(
      [&] { actual.value(); },
      [&](RuntimeStatusError const& ex) {
        EXPECT_EQ(500, ex.status().status_code());
        EXPECT_EQ("BAD", ex.status().error_message());
      },
      "exceptions are disabled: BAD \\[500\\]"
  );

  testing_util::ExpectException<RuntimeStatusError>(
      [&] { std::move(actual).value(); },
      [&](RuntimeStatusError const& ex) {
        EXPECT_EQ(500, ex.status().status_code());
        EXPECT_EQ("BAD", ex.status().error_message());
      },
      "exceptions are disabled: BAD \\[500\\]"
  );
}

TEST(StatusOrTest, ValueAccessorConstThrows) {
  StatusOr<int> actual(Status(500, "BAD"));

  testing_util::ExpectException<RuntimeStatusError>(
      [&] { actual.value(); },
      [&](RuntimeStatusError const& ex) {
        EXPECT_EQ(500, ex.status().status_code());
        EXPECT_EQ("BAD", ex.status().error_message());
      },
      "exceptions are disabled: BAD \\[500\\]"
  );

  testing_util::ExpectException<RuntimeStatusError>(
      [&] { std::move(actual).value(); },
      [&](RuntimeStatusError const& ex) {
        EXPECT_EQ(500, ex.status().status_code());
        EXPECT_EQ("BAD", ex.status().error_message());
      },
      "exceptions are disabled: BAD \\[500\\]"
  );
}

TEST(StatusOrTest, StatusConstAccessors) {
  StatusOr<int> const actual(Status(500, "BAD"));
  EXPECT_EQ(500, actual.status().status_code());
  EXPECT_EQ(500, std::move(actual).status().status_code());
}

TEST(StatusOrTest, ValueDeference) {
  StatusOr<std::string> actual("42");
  EXPECT_TRUE(actual.ok());
  EXPECT_EQ("42", *actual);
  EXPECT_EQ("42", std::move(actual).value());
}

TEST(StatusOrTest, ValueConstDeference) {
  StatusOr<std::string> const actual("42");
  EXPECT_TRUE(actual.ok());
  EXPECT_EQ("42", *actual);
  EXPECT_EQ("42", std::move(actual).value());
}

TEST(StatusOrTest, ValueArrow) {
  StatusOr<std::string> actual("42");
  EXPECT_TRUE(actual.ok());
  EXPECT_EQ(std::string("42"), actual->c_str());
}

TEST(StatusOrTest, ValueConstArrow) {
  StatusOr<std::string> const actual("42");
  EXPECT_TRUE(actual.ok());
  EXPECT_EQ(std::string("42"), actual->c_str());
}

TEST(StatusOrVoidTest, DefaultConstructor) {
  StatusOr<void> actual;
  EXPECT_FALSE(actual.ok());
  EXPECT_FALSE(actual.status().ok());
}

TEST(StatusOrVoidTest, StatusConstructorNormal) {
  StatusOr<void> actual(Status(404, "NOT FOUND", "It was there yesterday!"));
  EXPECT_FALSE(actual.ok());
  EXPECT_EQ(404, actual.status().status_code());
  EXPECT_EQ("NOT FOUND", actual.status().error_message());
  EXPECT_EQ("It was there yesterday!", actual.status().error_details());
}

TEST(StatusOrVoidTest, ValueConstructor) {
  StatusOr<void> actual(Status{});
  EXPECT_TRUE(actual.ok());
  testing_util::ExpectNoException([&] { actual.value(); });
  testing_util::ExpectNoException([&] { std::move(actual).value(); });
}

TEST(StatusOrVoidTest, ValueConstAccessors) {
  StatusOr<void> const actual(Status{});
  EXPECT_TRUE(actual.ok());
  testing_util::ExpectNoException([&] { actual.value(); });
  testing_util::ExpectNoException([&] { std::move(actual).value(); });
}

TEST(StatusOrVoidTest, ValueAccessorNonConstThrows) {
  StatusOr<void> actual(Status(500, "BAD"));

  testing_util::ExpectException<RuntimeStatusError>(
      [&] { actual.value(); },
      [&](RuntimeStatusError const& ex) {
        EXPECT_EQ(500, ex.status().status_code());
        EXPECT_EQ("BAD", ex.status().error_message());
      },
      "exceptions are disabled: BAD \\[500\\]"
  );

  testing_util::ExpectException<RuntimeStatusError>(
      [&] { std::move(actual).value(); },
      [&](RuntimeStatusError const& ex) {
        EXPECT_EQ(500, ex.status().status_code());
        EXPECT_EQ("BAD", ex.status().error_message());
      },
      "exceptions are disabled: BAD \\[500\\]"
  );
}

TEST(StatusOrVoidTest, ValueAccessorConstThrows) {
  StatusOr<void> actual(Status(500, "BAD"));

  testing_util::ExpectException<RuntimeStatusError>(
      [&] { actual.value(); },
      [&](RuntimeStatusError const& ex) {
        EXPECT_EQ(500, ex.status().status_code());
        EXPECT_EQ("BAD", ex.status().error_message());
      },
      "exceptions are disabled: BAD \\[500\\]"
  );

  testing_util::ExpectException<RuntimeStatusError>(
      [&] { std::move(actual).value(); },
      [&](RuntimeStatusError const& ex) {
        EXPECT_EQ(500, ex.status().status_code());
        EXPECT_EQ("BAD", ex.status().error_message());
      },
      "exceptions are disabled: BAD \\[500\\]"
  );
}

TEST(StatusOrVoidTest, StatusConstAccessors) {
  StatusOr<void> const actual(Status(500, "BAD"));
  EXPECT_EQ(500, actual.status().status_code());
  EXPECT_EQ(500, std::move(actual).status().status_code());
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
