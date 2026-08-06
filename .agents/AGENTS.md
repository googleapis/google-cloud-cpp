# Agent Guidelines for google-cloud-cpp

## C++ Testing Guidelines

- Use `EXPECT_THAT` / `ASSERT_THAT` with gtest/gmock matchers (e.g.,
  `testing::Eq`, `testing::HasSubstr`, `testing::IsEmpty`, `testing::NotNull`)
  rather than calling functions on the test assertions.
- Use `EXPECT_THAT(status, IsOk())` or `ASSERT_THAT(status_or, IsOk())` for
  Status / StatusOr checks (using `google::cloud::testing_util::IsOk`).
- Use `EXPECT_TRUE` / `ASSERT_TRUE` (or `EXPECT_FALSE` / `ASSERT_FALSE`) for
  standard boolean expression checks (e.g., `stream.good()`).
- **Prefer Declarative Container Matchers Over Imperative Loops:** Avoid manual
  loops (`for (...)`), boolean search flags (`bool found = false;`), and manual
  container filtering in tests. Use GoogleTest container matchers (e.g.,
  `testing::Contains`, `testing::Each`, `testing::ElementsAre`,
  `testing::UnorderedElementsAre`, `testing::IsEmpty`, `testing::SizeIs`) to
  express collection assertions declaratively.
- **Write Custom Matchers for Complex Objects and Protobufs:** When validating
  complex objects, structs, or protobuf messages (e.g., time series, spans,
  requests, responses), define custom matchers using `MATCHER_P` / `MATCHER_P2`
  with `ExplainMatchResult`. Compose them using `testing::AllOf`,
  `testing::AnyOf`, `testing::Property`, and `testing::Field`.
  - *Why:* Declarative matchers provide detailed diagnostic mismatch
    explanations when tests fail, whereas boolean flags only output
    `Value of: found, Actual: false, Expected: true`.
- **Example Pattern:**
  ```cpp
  MATCHER_P(MetricType, matcher, "") {
    return ExplainMatchResult(matcher, arg.metric().type(), result_listener);
  }

  MATCHER_P2(HasMetricLabel, key, val_matcher, "") {
    auto const& labels = arg.metric().labels();
    auto it = labels.find(key);
    if (it == labels.end()) {
      *result_listener << "no metric label '" << key << "'";
      return false;
    }
    return ExplainMatchResult(val_matcher, it->second, result_listener);
  }

  // Composed assertion:
  EXPECT_THAT(recorded_metrics, Contains(HasTimeSeries(AllOf(
      MetricType(HasSubstr("outstanding_rpcs")),
      HasMetricLabel("channel_pool_lb_policy", Eq("RANDOM_TWO_LEAST_USED"))))));
  ```
