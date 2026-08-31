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
  EXPECT_THAT(recorded_metrics, Contains(AllOf(
      MetricType(HasSubstr("outstanding_rpcs")),
      HasMetricLabel("channel_pool_lb_policy", Eq("RANDOM_TWO_LEAST_USED")))));
  ```

## Type Deduction and `auto` Guidelines (Abseil Tip #232)

Follow [Abseil Tip of the Week #232](https://abseil.io/tips/232) and the Google
C++ Style Guide: use `auto` only if it makes code clearer or safer, not merely
to avoid typing an explicit type.

- **Range-Based `for` Loops Over Maps and Associative Containers:**
  - Use `auto` with structured bindings (`for (auto const& [key, value] : map)`)
    when iterating over `std::map`, `absl::flat_hash_map`, protobuf maps (e.g.,
    `metadata()`, `labels()`), and JSON `.items()`.
  - *Why:* Iterating with `std::pair<Key, Value>` triggers implicit conversions
    and unintentional deep copies because map elements are
    `std::pair<const Key, Value>`. Structured bindings eliminate this hazard and
    remove `kv.first` / `kv.second` noise.
- **Standard Factory Functions:**
  - Use `auto` when the type is explicitly specified on the RHS with standard
    factory functions (e.g., `auto client = std::make_shared<MockClient>();`,
    `auto ptr = std::make_unique<T>(...);`).
- **Iterators:**
  - Use `auto` for iterator variables when the container type is clearly
    declared in the local scope (`auto it = local_vec.begin();`).
  - When the container is not local (e.g., a class member variable), either
    spell out the iterator type or explicitly bind the dereferenced element type
    (e.g., `ElementType const& elem = *it;`).
- **Spell Out Domain Types, Protobufs, and Return Values:**
  - Do not use `auto` where it obscures domain types, protobuf messages, or
    function return types (e.g., avoid `auto actual = client.InsertObject(...)`;
    use `StatusOr<ObjectMetadata> actual = ...`).
  - Do not use `auto` for nested protobuf access (e.g., avoid
    `auto const& field = proto.nested().field()`; spell out the protobuf/string
    type).
- **Avoid `auto` for Primitive / Numeric Types:**
  - Use explicit types (`std::size_t`, `std::int64_t`, `std::uint32_t`, etc.)
    instead of bare `auto` initialized with integer literals.
- **Explicit Semantics (`const`, `&`, `*`):**
  - When a reference or pointer is intended, always explicitly qualify as
    `auto const&`, `auto&`, or `auto*` to prevent accidental copies (since bare
    `auto` deduces by value) and to make ownership and mutability unambiguous.

## Default Arguments in Internal Namespaces

- **Avoid Default Parameters in `internal` Namespaces:**
  - Functions, member functions, and constructors declared in `internal`
    namespaces (e.g., `google::cloud::*::internal`, `google::cloud::*_internal`)
    must not specify default parameter values.
  - Require all internal callers and unit tests to explicitly specify all
    arguments.
  - *Why:* Default arguments in internal implementation code obscure
    dependencies at call sites, make it easy to inadvertently omit required
    configuration, and can mask untested branches in unit tests. If a simpler
    calling convention is genuinely needed, provide an explicit overload.
