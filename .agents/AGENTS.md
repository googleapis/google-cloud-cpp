# Agent Guidelines for google-cloud-cpp

## C++ Testing Guidelines

- Use `EXPECT_THAT` / `ASSERT_THAT` with gtest/gmock matchers (e.g.,
  `testing::Eq`, `testing::HasSubstr`, `testing::IsEmpty`, `testing::NotNull`)
  rather than calling functions on the test assertions.
- Use `EXPECT_THAT(status, IsOk())` or `ASSERT_THAT(status_or, IsOk())` for
  Status / StatusOr checks (using `google::cloud::testing_util::IsOk`).
- Use `EXPECT_TRUE` / `ASSERT_TRUE` (or `EXPECT_FALSE` / `ASSERT_FALSE`) for
  standard boolean expression checks (e.g., `stream.good()`).
