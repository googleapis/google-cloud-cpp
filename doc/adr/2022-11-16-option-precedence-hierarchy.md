**Title**: Establish how multiple values for the same option are reconciled.

**Status**: proposed

**Context**: There are a variety of ways that options can be customized for a
given rpc call. Both `Client` and `Connection` classes can be created with a set
of `google::cloud::Options`. Individual function calls in a `Client` can be
passed `google::cloud::Options`. Some environment variables can be set that
such that the variable value sets an Option associated with that environment
variable. Lastly, GCS `Request` types inherit from various Option templates
which allow the corresponding option to be set in an instance of the `Request`.

When the same Option is set, with different values, via more than one of these
mechanisms, there needs to be a uniform process of determining which value to
use.

**Decision**: The following hierarchy will be followed when determining which
value to use when the same Option is set via multiple mechanisms:

1. Environment variables
1. Per function call/GCS `Request` instance (if both are set with different values, an error is returned)
1. `Client` Options
1. `Connection` Options
1. Default values for Options

**Consequences**: For the most part, this hierarchy is already applied. However,
it may result in some behavioral changes for existing users, particularly in the
case where both "per call" and `Request` options are set to different values.
