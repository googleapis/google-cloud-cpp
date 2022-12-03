**Title**: Establish how multiple values for the same option are reconciled.

**Status**: proposed

**Context**: There are a variety of ways that options can be customized for a
given function call. Both `Client` and `Connection` classes can be created with
`google::cloud::Options`. Individual function calls in a `Client` can be passed
`google::cloud::Options`. Some environment variables can be set such that the
variable value sets an option associated with that environment variable. Lastly,
the storage library predates the `google::cloud::Options` mechanism, and many of
the functions in this library consume a parameter pack that sets optional values
for the RPCs used in the function.

When the same option is set, with different values, via more than one of these
mechanisms, there needs to be a uniform process of determining which value to
use.

**Decision**: The following priorities will be followed when determining which
value to use when the same option is set via multiple mechanisms:

1. Environment variables
1. Per function call/GCS `Request` instance (if both are set with different values, an error is returned)
1. `<service>Client` constructor `Options`
1. `Make<service>Connection` factory function `Options`
1. Default values

**Consequences**: The intent was always to follow the priority defined here.
Thus:

- Failure to detect conflicts is a bug in the library.
- Any changes in behavior are bug fixes.
- The number of conflicts is expected to be small as the overlap is tiny and the features are rarely used.
