**Title**: customers must type `experimental` to access experimental features.

**Status**: accepted

**Context**: From time-to-time we need to add experimental features to existing
libraries. These may be:

- Improvements or additions that do not have a stable API. Normally we have a
  period with new libraries where we let the API stabilize, but we have no such
  luxury when adding APIs to a GA library.
- Features of a service that are released as "beta" or "EAP", or otherwise not
  GA. We may want to give folks access to that feature, but again the API might
  change, or the underlying feature may be completely removed.

Making changes to GA libraries *implies*, unless we take additional measures,
that the changes are GA.

The `[[deprecated]]` attribute is not available in C++11, but even if we use
existing workarounds (e.g. `ABSL_DEPRECATED()`), the generated warning messages
are misleading; they include the literal word `deprecated` in the warnings.

Sometimes it is possible to add new features in a separate namespace, such as
`google::cloud::experimental`, or `google::cloud::${service}_experimental`.
That is often cumbersome for features added to a core class like
`google::cloud::${service}::Client` or `google::cloud::${service}::Connection`.

We could use a separate branch for experimental features, but then customers
would need to manually diff the branch to understand what is experimental and
what is stable. And we would need to support twice as much code.

**Decision**: Experimental features in existing GA libraries **must** require
the application developer to type `experimental` (or `Experimental`, or whatever
casing is correct under the applicable naming convention) to use them.

Examples of acceptable approaches include:

- Adding any new symbols to the `google::cloud::${service}_experimental`
  namespace.
- Naming the symbol `ExperimentalFoo` or similar.
- Functions can require a non-defaulted parameter of type
  `google::cloud::ExperimentalTag`. This type cannot be created with a brace
  initializer, the caller would need to type `Experimental` when using such
  functions.
- Adding new libraries, where the target name includes `experimental-`. For
  CMake we generally use `google-cloud-cpp::experimental-${library}`, for
  Bazel we generally use `//:experimental-${library}` (note this is a top-level)
  target.

**Consequences**: This decision makes it possible to add experimental features
while clearly signalling to any users of the feature that it is indeed
experimental and subject to change.  Customers can use these new features, but
are forewarned about their stability.  Without this, we would not be able to add
features until we are ready to commit to the final, stable API for it. The only
possible way to do so would be a "preview" branch, maybe without releases. This
is very hard for customers to use, and does not clearly delineate the stable
vs. experimental parts of the branch.
