# Supporting C++ without exceptions

This document describes how we support C++ with exceptions disabled in the
Google Cloud C++ libraries.  It is aimed at the project developers who may
need to raise exceptions or write tests that verify the exceptions are raised.

## Background

We expect that *some* (but not the majority) of the users of the Google Cloud
C++ libraries will want to compile their code without exception support, notably
Google does not use C++ exceptions, and many libraries that are created by
Google do not support exceptions either.  On the other hand, we expect that most
C++11 users will be more familiar with exceptions as the error reporting
mechanism, we want to offer these users (most likely the majority) and good
experience on the platform.

Documenting guidelines as to when is it a good idea to raise exceptions vs.
returning and error status is beyond the scope of this document, and is
described in the [style guide](cpp-style-guide.md).

## Overview

In general, the library will not raise or catch exceptions directly. When
exceptions are disabled the library will log the problem and then call
`std::abort()`. The tests will need to use `EXPECT_DEATH()` when exceptions are
disabled, and `EXPECT_THROW()` in the normal case. The examples should use a 
`try/catch` block in `main()`, following best practices, therefore they cannot
be compiled when exceptions are disabled.

Some libraries may need to support users that cannot use exceptions themselves.
In such cases we suggest that the library creates a separate set of APIs, in a
separate namespace, to support these users.  We recommend that libraries expose
the main idiomatic API in their examples and documentation.

## Detailed Design

In this document we will use the `bigtable::` as an example.  Each library will
use their own namespace.  Some of this code should be refactored to a common
library once that becomes necessary.

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

### Offering APIs with Exceptions Disabled

We will use [`bigtable::Table::ReadRow`][bigtable-doxygen-link] function as an
example.  This is a wrapper around
[`google.bigtable.v2.Table.ReadRows`][bigtable-proto-link] it makes multiple
attempts to contact the server, if successful, it returns a 2-tuple,
the first element in the tuple is a boolean that indicates if the row exists,
the second is the value of the row. An exception is raised only if the function
cannot contact the server, or if fetching the value fails for some reason.

[bigtable-doxygen-link]: https://GoogleCloudPlatform.github.io/google-cloud-cpp/
[bigtable-proto-link]: https://github.com/googleapis/googleapis/blob/master/google/bigtable/v2/bigtable.proto

Note that normal "failures" (the row does not exist), do not result in an
exception being raised.  The function is defined as "find out if the row exists
and return its value".  Therefore, a "row does not exist" condition is not
enough to raise exceptions, while failing to contact the server is, because the
result is unknown.

The function definition then reads something like this:

```C++
namespace bigtable {
class Table { public:
  // Many details ommitted
  std::pair<bool, Row> ReadRow(std::string key, Filter filter);
};
}
```

If the library must offer APIs without exceptions then they should create a
separate namespace for them.  We do not prescribe what that namespace should be,
but as an example:

```C++
namespace bigtable {
namespace noex {
class Table { public:
  // Return status != grpc::Status::OK if it could not do its job.
  std::pair<bool, Row> ReadRow(std::string key, Filter filter, grpc::Status& status) {
    // Complex implementation here ...
  }
};
}  // namespace noex
}  // namespace bigtable
```

The API exposed to the majority of the users can be easily implemented as:

```C++
namespace bigtable {
class Table { public:
  std::pair<bool, Row> ReadRow(std::string key, Filter filter) {
    grpc::Status status;
    auto result = impl_->ReadRow(std::move(key), std::move(filter), status);
    if (not status.ok()) {
      internal::RaiseGrpcError(status, "Table::ReadRow()");
    }
    return result;
  }
 private:
  noex::Table impl_;
};
}  // namespace bigtable
```

#### Iterators

APIs that iterate over results cannot simply get an extra status parameter
or return an extra result for that matter: the errors may not be detected
until much later.  This technique can be extended for such APIs with
some caveats: if the application fails to retrieve the error result the
library should call ``
loops.  We recommend that the library implements the following APIs in this
case:

```C++
namespace bigtable {
namespace noex {
class RowReader {
public:
  // If true, RowReader() calls internall:Raise*(), otherwise, sets status().
  RowReader(bool raise_on_error);
  
  ~RowReader() {
    if (not error_retrieved_ and not status_.ok()) {
      // call internal::RaiseGrpcError() anyway, cannot silently ignore errors.
    }
  }
  
  iterator begin() const;
  iterator end() const;
  
  grpc::Status Finish() {
    error_retrieved_ = true;
    return status_;
  }
};

class Table { public:
  // Applications that do not want exceptions set raise_on_error == false
  RowReader ReadRows(RowRange, Filter filter, bool raise_on_error);
 private:
  noex::Table impl_;
};
}  // namespace noex

class Table { public:
  RowReader ReadRows(RowRange range, Filter filter) {
    return impl_->ReadRows(std::move(range), std::move(filter), true); 
  }
};
}  // namespace bigtable
```

The applications that use exceptions can then use the iterator as in:

```C++
  for (auto& row : table->ReadRows(...)) {
    // stuff here
  }
```

Applications that cannot use exceptions need to exercise more caution:

```C++
  auto reader = table->ReadRows(...);
  for (auto& row : reader) {
    // stuff here
  }
  auto status = reader.Finihs();
  if (not status.ok()) {
    // handle the error.
  }
```

#### Downsides

This is not a perfect solution. Some APIs do not work well with an extra
parameter, for example, iterators are not easy to modify this way. In addition,
because the majority of the code will be written with exceptions in mind there
may be places that signal errors directly.  This is no different than an
`assert()` or a `ABSL_RAW_CHECK()`.


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
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(foo(), std::runtime_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(foo(), "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
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
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(bar(), std::runtime_error);
  EXPECT_EQ(1, counter);
#else
  EXPECT_DEATH_IF_SUPPORTED(bar(), "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}
```

Note that the value of `counter` is unchanged with `EXPECT_DEATH()`.  Since some
of these tests become trivial sometimes it is simpler to just say:

```C++
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
// Test is trivial without exceptions, 
TEST(BarTest, Increment) {
  counter = 0;
  EXPECT_THROW(bar(), std::runtime_error);
  EXPECT_EQ(1, counter);
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
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
