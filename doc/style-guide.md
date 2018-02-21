# C++ Style Guide for google-cloud-cpp

<!-- TODO(#70) - complete this style guide, see #98 for a proposal. -->

This project follows the [Google Style Guide][google-style-guide-link], with one
major exception: exceptions are allowed.

[google-style-guide-link]: https://google.github.io/styleguide/cppguide.html

## When to raise an Exception

There are no hard and fast rules as to when is better to raise an exceptions vs.
returning an error status, specially for libraries that contact remote systems.
We offer the following:

```
Raise an exception when the function could not complete its work.
```

This is typically a relatively easy easy to do for purely local functions. If
the preconditions are not met, or the arguments are out of range or invalid one
should raise an exception. It is a harder rule to apply for operations that
need to contact a remote server. The following list is not exhaustive, but
provides some general principles:

- Failing to contact the server should raise an exception.
- Receiving an invalid response from the server should raise an exception.
- Receiving a permanent error status from the server should raise an exception.
- Being unable to find an object, or row, or file in a remote server should
  not raise an exception.

As we said, there are no hard and fast rules, consult your colleagues and be
ready to change your mind.
