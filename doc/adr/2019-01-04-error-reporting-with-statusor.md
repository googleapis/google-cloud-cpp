**Title**: Functions should report errors using a `StatusOr<T>` instead of
throwing exceptions.

**Status**: proposed

**Context**: We expect there to be users of this library who use C++
exceptions, as well as those who do not. The most recent [ISO C++
survey](https://isocpp.org/blog/2018/03/results-summary-cpp-foundation-developer-survey-lite-2018-02)
suggests that there are ~50% of users who are allowed to use C++ exceptions,
~20% who cannot, and about %30 who can use exceptions in some places but not
others. We must provide a library that works for all users, whether or not they
can use exceptions.

**Decision**: All of our APIs will report errors by returning a `StatusOr<T>`
object, which will indicate whether the requested `T` object was returned or
whether the function failed and returned a `Status` instead. None of our APIs
will throw exceptions to indicate errors.

**Consequences**: The decision to report all errors as a `StatusOr<T>` return
value supports 100% of potential users, because it works in any environment,
regardless of the user's ability to use C++ exceptions. Additionally, since
`StatusOr<T>::value()` may `throw` (if compiled with `-fexceptions`) it can be
used without explicit error checking in environments where callers want
exceptions to be thrown anyway.

A downside of this decision is that for the ~50% of users who might prefer an
exception-based API, the `StatusOr<T>` interface (even with the call to a
throwing `value()`) is not as natural or idiomatic as a well designed
exception-based API. More generally, the decision not to throw affects many
facets of the API's design. For example, since constructors cannot throw (by
virtue of this ARD) APIs may lean more heavily on factory functions rather than
constructors for creating objects. This may result in a less natural and more
verbose expression of code intent.

