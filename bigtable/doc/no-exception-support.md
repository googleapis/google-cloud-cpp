# Supporting C++ without exceptions

This document describes how we support C++ with exceptions disabled in the
Cloud Bigtable C++ libraries.  It is aimed at the project developers who may
need to raise exceptions or write tests that verify the exceptions are raised.

## Background

We expect that some of the users of the Cloud Bigtable C++ client will want to
compile their code without exception support, notably Google and many libraries
that are created by Google.  On the other hand, we expect that most C++11 users
will be more familiar with exceptions as the error reporting mechanism, we want
to offer these users (most likely the majority) and good experience on the
platform.

Documenting guidelines as to when is it a good idea to raise exceptions vs.
returning and error status is beyond the scope of this document, and is
described in the [style guide](../../doc/style-guide.md).

## Overview

In general, the library will not raise or catch exceptions directly. When
exceptions are disabled the library will log the problem and then call
`std::abort()`. The tests will need to use `EXPECT_DEATH()` when exceptions are
disabled, and `EXPECT_THROW()` in the normal case. The examples should use a 
`try/catch` block in `main()`, following best practices, therefore they cannot
be compiled when exceptions are disabled.

## Detailed Design

### Raising Exceptions

The library should not `throw` directly, instead we use a delegate function to
raise an exception.  For example, this code:

```C++
void foo() {
  throw std::runtime_error("oh no!");
}
```

becomes:

```C++
void foo() {
  bigtable::internal::RaiseRuntimeError("oh no!");
}
```

The `bigtable::internal::Raise*` functions will either raise an exception or
call `std::abort()` depending on whether exceptions are enabled.  The library
only implements the minimum set of delegate functions that it needs, developers
should add new delegates when raising a new exception type.

### Catching Exceptions in the Library

The library should never catch exceptions.  The library should offer the
[strong exception guarantee][cpp-reference-exceptions] and simply rollback to
the previous (**local**) state when an exception is raised.  If a function
cannot offer the strong exception guarantee the library **must** offer the
[basic exception guarantee][cpp-reference-exceptions] and note this as a
limitation in the documentation of said function.  We expect this to be
exceedingly rare, as the library is largely stateless.

[cpp-reference-exceptions]: http://en.cppreference.com/w/cpp/language/exceptions

### Unit Tests

Unit tests cannot use `EXPECT_NO_THROW()`, the expression should simply be
called directly.

Unit tests that trigger errors should use `EXPECT_DEATH_IF_SUPPORTED()` when
exceptions are disabled.  For example, this code:

```C++
TEST(MyTest, TheTest) {
  EXPECT_THROW(foo(), std::runtime_error);
}
```

becomes:

```C++
TEST(MyTest, TheTest) {
#if ABSL_HAVE_EXCEPTIONS
  EXPECT_THROW(foo(), std::runtime_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(foo(), "exceptions are disabled");
#endif  // ABSL_HAVE_EXCEPTIONS
}
```

Many tests need to take different action when exceptions are disabled. The
implementation of `EXPECT_DEATH_*` runs the provided expression (`foo()` in the
example) in a separate process, so any side-effects of the function, including
calls to `googlemock` objects are not observable. One must modify the tests to
either act differently when exceptions are disabled, or simply disable the test.
For example, suppose one had a test like this:

```C++
static int counter = 0;
void bar() {
  if (counter++ % 3 == 0) {
    bigtable::internal::RaiseRuntimeError("it was time");
  }
}

TEST(BarTest, Increment) {
  counter = 0;
  EXPECT_THROW(bar(), std::runtime_error);
  EXPECT_EQ(1, counter);
}
```

Such a test would become:

```C++
// ... bar and counter definition as above ...

TEST(BarTest, Increment) {
  counter = 0;
#if ABSL_HAVE_EXCEPTIONS
  EXPECT_THROW(bar(), std::runtime_error);
  EXPECT_EQ(1, counter);
#else
  EXPECT_DEATH_IF_SUPPORTED(bar(), "exceptions are disabled");
#endif  // ABSL_HAVE_EXCEPTIONS
}
```

Note that the value of `counter` is unchanged with `EXPECT_DEATH()`.  Since some
of these tests become trivial sometimes it is simpler to just say:

```C++
#if ABSL_HAVE_EXCEPTIONS
// Test is trivial without exceptions, 
TEST(BarTest, Increment) {
  counter = 0;
  EXPECT_THROW(bar(), std::runtime_error);
  EXPECT_EQ(1, counter);
}
#endif  // ABSL_HAVE_EXCEPTIONS
```

### Examples

In general we want our examples to be easy to read, and to show best practices,
cluttering them with `#if`/`#endif` blocks is counter productive.  Therefore,
we recommend that tests always assume the code is compiled with exceptions,
and that they use `try`/`catch` blocks to detect and report errors. The CMake
files can disable specific tests using `GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS`,
for example:

```cmake
if (GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    add_executable(foo foo.cc)
    target_link_libraries(foo bigtable::client)
endif (GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
```
