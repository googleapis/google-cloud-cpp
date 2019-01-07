**Title**: Functions should report errors using `StatusOr<T>` *instead of*
throwing exceptions.

**Status**: proposed

**Context**: We know there will be users of these C++ libraries who want to use
C++ exceptions as well as those who are not able to. Our C++ libraries must
work for all of our users, regardless of their ability to use exceptions.

**Decision**: All of our APIs will report errors to callers by returning a
`StatusOr<T>` object, which will indicate whether the function successfully
returned the requested `T` object, or whether the function failed and returned
an error `Status` instead. None of our APIs will throw exceptions to indicate
errors.

**Consequences**: This decision will result in a single set of APIs and a
consistent vocabulary for all users, whether or not they choose to compile with
exceptions. Indeed, this decision does not prevent callers from using
exceptions in their own code.

A downside of this decision is that our APIs will not be natrual or idiomatic
for the [50+%][survey-link] of users who might prefer exceptions for error
reporting.

Changing existing APIs from throwing exceptions to returning `StatusOr<T>` is a
breaking change. As of this writing (Jan 2019), this project has a [Google
Cloud Storage][gcs-link] component that is at the Alpha quality level, and a
[Google Cloud Bigtable][bigtable-link] that is already at the Beta quality
level. Since neither of these are at the GA quality level, breaking changes are
allowed. However, we still want to minimize the disruption caused by these
changes, especially for the Bigtable library, which is Beta. For Bigtable, we
will communicate the upcoming changes to the users and try to get them
implemented over the coming months.


[gcs-link]: https://github.com/GoogleCloudPlatform/google-cloud-cpp/tree/master/google/cloud/storage
[bigtable-link]: https://github.com/GoogleCloudPlatform/google-cloud-cpp/tree/master/google/cloud/bigtable
[survey-link]: https://isocpp.org/blog/2018/03/results-summary-cpp-foundation-developer-survey-lite-2018-02

