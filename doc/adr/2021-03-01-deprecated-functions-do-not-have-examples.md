**Title**: Deprecated functions do not have examples.

**Status**: accepted

**Context**: we write examples for all important functions, typically the ones
that result in one or more RPCs. From time to time we need to deprecate a
function, maybe because the function has a serious defect that cannot be
resolved without changing its API, or because the function represents an
experiment that "failed", or because we found a better way to do things.

**Decision**: when we deprecate a function we will remove the examples to that
function. If the function is being replaced, we will reference the replacement
in the "Examples" paragraph. If the function is being removed, we will simply
remove the examples before the function itself. Unit tests and integration
tests are preserved. While a function might be deprecated, it is expected to
work.

**Consequences**: customers will be directed to the improved replacement,
customers will not be misguided by examples that point to a deprecated approach.
Developers of `google-cloud-cpp` will have fewer examples to maintain. Customers
that cannot migrate to the new approach will not find examples of the approach
they are currently using, this is mitigated by (a) they already are using the
old approach, and (b) they can find the examples in the documentation
accompanying old releases.
