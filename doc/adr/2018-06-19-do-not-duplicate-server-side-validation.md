**Title**: Do not duplicate validation performed by the server-side.

**Status**: accepted.

**Context**: many APIs impose restrictions on their parameters. Names may have
length limits, or strings may need to conform to specific formats, or ranges
must be non-empty.  It is tempting to validate these arguments in the
client-side, since we *know* the API will fail if it is allowed to continue
with the invalid parameters. However, any such validation is redundant, since
the server is going to validate its inputs as well. Furthermore, any such
validation is wasteful: most of the time the application will pass on valid
arguments, checking them twice is just wasting client-side CPU cycles. Moreover,
the server-side is the source of truth, so having the validations performed on
the client side wil require extra work if the server restrictions are ever
modified.

**Decision**: the client libraries do not validate any of the request
parameters beyond what is required to avoid local crashes.

**Consequences**: the main downsides are:

- The error messages could be better if generated locally.
- The server must validate all inputs correctly.
- Sometimes, we will need to validate inputs to avoid library crashes, and
  that will be confusing because it will seem like a violation of this ADR.
