# Agent Guidelines for google-cloud-cpp

## C++ Testing Guidelines

- Use `EXPECT_THAT` / `ASSERT_THAT` with gtest/gmock matchers (e.g.,
  `testing::Eq`, `testing::HasSubstr`, `testing::IsEmpty`, `testing::NotNull`)
  rather than calling functions on the test assertions.
- Use `ASSERT_STATUS_OK(status_or)` or `EXPECT_STATUS_OK(status)` for `Status` /
  `StatusOr` checks.
- Use `EXPECT_TRUE` / `ASSERT_TRUE` (or `EXPECT_FALSE` / `ASSERT_FALSE`) for
  standard boolean expression checks (e.g., `stream.good()`).
