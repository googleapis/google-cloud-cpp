## The GCS Library retries all operations.

**Status**: accepted

**Context**: operations that change state in the server may complete with an
indeterminate status code. For example: an error in the 500 range may be
produced by a middle-tier server after the operation was started by the backend.
Re-issuing the request may not work as expected for the application developer.

**Decision**: by default the library will retry all requests, including
non-idempotent ones. The library will allow application developers to override
the definition of what operations are automatically retried.

**Consequences**: most operations become easier to use for application
developers. In very rare cases the operation will result in double uploads, or
in a new generation of the object or metadata being created. In even more
rare cases the operation may fail, for example, an operation to create an object
with `IfGenerationMatch(0)` would fail on the second attempt.

