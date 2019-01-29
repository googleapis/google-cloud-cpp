**Title**: Functions should report errors using `StatusOr<T>` *instead of*
throwing exceptions.

**Status**: accepted

**Context**: We know there will be users of these C++ libraries who want to use
C++ exceptions as well as those who are not able to. Our C++ libraries must
work for all of our users, regardless of their ability to use exceptions.

**Decision**: None of our APIs will throw exceptions to indicate errors.
Instead, our APIs will typically report errors to callers by returning a
`Status` or `StatusOr<T>` object, unless the library we're using has another
non-throwing way to report errors (e.g., [badbit][badbit-link] in the standard
I/O library).

**Consequences**: This decision will result in a single set of APIs and a
consistent vocabulary for all users, whether or not they choose to compile with
exceptions. This decision does not prevent callers from using exceptions in
their own code.

A downside of this decision is that our APIs will not be natural or idiomatic
for the [50+%][survey-link] of users who might prefer exceptions for error
reporting.

Changing existing APIs from throwing exceptions to returning `StatusOr<T>` is a
breaking change. As of this writing (Jan 2019), this project has a [Google
Cloud Storage][gcs-link] component that is at the Alpha quality level, and a
[Google Cloud Bigtable][bigtable-link] that is already at the Beta quality
level. We plan to immediately change the API for Google Cloud Storage. We have
no immediate plans to change the API for Cloud Bigtable. We will communicate a
timeline to change this API in a separate document.

[badbit-link]: https://en.cppreference.com/w/cpp/io/ios_base/iostate
[gcs-link]: https://github.com/googleapis/google-cloud-cpp/tree/master/google/cloud/storage
[bigtable-link]: https://github.com/googleapis/google-cloud-cpp/tree/master/google/cloud/bigtable
[survey-link]: https://isocpp.org/blog/2018/03/results-summary-cpp-foundation-developer-survey-lite-2018-02

